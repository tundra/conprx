//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/lpc.hh"
#include "binpatch.hh"
#include "utils/log.hh"

using namespace conprx;
using namespace lpc;
using namespace tclib;

PortView::PortView()
  : calibration_result_(F_FALSE)
  , calibration_capbuf_(NULL)
  , handle_(INVALID_HANDLE_VALUE) { }

PatchingInterceptor::PatchingInterceptor(handler_t handler)
  : handler_(handler)
  , patches_(Platform::get(), Vector<PatchRequest>(patch_requests_, kPatchRequestCount))
  , one_shot_special_handler_(false)
  , is_locating_cccs_(false)
  , locate_cccs_result_(F_FALSE)
  , cccs_(NULL)
  , locate_cccs_port_handle_(INVALID_HANDLE_VALUE)
  , is_determining_base_port_(false)
  , determine_base_port_result_(F_FALSE)
  , base_port_handle_(INVALID_HANDLE_VALUE)
  , active_calibration_(NULL) {
  CHECK_EQ("stack grows up", -1, infer_stack_direction());
}

ntstatus_t NTAPI PatchingInterceptor::nt_request_wait_reply_port_bridge(handle_t port_handle,
    lpc::relevant_message_t *request, lpc::relevant_message_t *incoming_reply) {
  // Similarly to Interceptor::calibrate we deliberately do the work here up
  // to the point where we capture the stack trace in a static function such
  // that we can check that there's a call to it on the stack. Whatever happens
  // after the stack capture doesn't matter but just for simplicity we keep all
  // the calibration stuff here so the instance method can ignore that
  // completely. Also, for this to work we need to be sure the compiler
  // materializes the call to this function since otherwise it doesn't show up
  // in the stack trace. When it simply delegated to the instance method the
  // optimizing compiler made the call into a jump, which is good in general but
  // not what we want here.
  PatchingInterceptor *inter = current();
  if (inter->one_shot_special_handler_) {
    inter->one_shot_special_handler_ = false;
    if (request->api_number == kGetConsoleCPApiNum && inter->is_locating_cccs_) {
      void *scratch[kLocateCCCSStackCaptureSize];
      Vector<void*> stack(scratch, kLocateCCCSStackCaptureSize);
      memset(stack.start(), 0, stack.size_bytes());
      if (!inter->capture_stacktrace(stack))
        // If we can't capture the stack we just clear out the variable and
        // keep going.
        stack = Vector<void*>();
      inter->locate_cccs_result_ = inter->process_locate_cccs_message(port_handle, stack);
      return NtStatus::success().to_nt();
    } else if (request->api_number == kGetProcessShutdownParametersApiNum && inter->is_determining_base_port_) {
      inter->determine_base_port_result_ = inter->process_base_request_message(port_handle, request);
      return NtStatus::success().to_nt();
    } else if (inter->active_calibration() != NULL && request->api_number == inter->active_calibration()->apinum()) {
      inter->active_calibration()->process_calibration_message(port_handle, request);
      return NtStatus::success().to_nt();
    } else {
      return inter->nt_request_wait_reply_port_imposter(port_handle, request, incoming_reply);
    }
  } else {
    return inter->nt_request_wait_reply_port(port_handle, request, incoming_reply);
  }
}

ntstatus_t PatchingInterceptor::nt_request_wait_reply_port(handle_t port_handle,
    relevant_message_t *request, relevant_message_t *reply) {
  if (enabled_) {
    lpc::Message::Destination destination;
    if (port_handle == console_port_handle()) {
      destination = lpc::Message::Destination::mdConsole;
    } else if (port_handle == base_port_handle_) {
      destination = lpc::Message::Destination::mdBase;
    } else {
      // TODO: what's a cleaner way to express this control flow that doesn't
      //   use goto?
      goto pass_through;
    }
    lpc::Message message(port_handle, this, port_xform(), destination, request, reply);
    return handler_(&message).to_nt();
  }
 pass_through:
  return nt_request_wait_reply_port_imposter(port_handle, request, reply);
}

fat_bool_t PatchingInterceptor::initialize_patch(PatchRequest *request,
    const char *name, address_t replacement, module_t ntdll) {
  address_t original = Code::upcast(GetProcAddress(ntdll, name));
  if (original == NULL) {
    WARN("GetProcAddress(-, \"%s\"): %i", name, GetLastError());
    return F_FALSE;
  }
  *request = PatchRequest(original, replacement);
  return F_TRUE;
}

NtStatus PatchingInterceptor::call_native_backend(handle_t port,
    lpc::relevant_message_t *request, lpc::relevant_message_t *incoming_reply) {
  ntstatus_t result = nt_request_wait_reply_port_imposter(port, request, incoming_reply);
  return NtStatus::from_nt(result);
}

fat_bool_t PatchingInterceptor::install() {
  module_t ntdll = GetModuleHandle(TEXT("ntdll.dll"));
  if (ntdll == NULL) {
    WARN("GetModuleHandle(\"ntdll.dll\"): %i", GetLastError());
    return F_FALSE;
  }

#define __INIT_PATCH__(Name, name, RET, SIG, ARGS)                             \
  F_TRY(initialize_patch(name##_patch(), #Name, Code::upcast(name##_bridge), ntdll));
  FOR_EACH_LPC_FUNCTION(__INIT_PATCH__)
#undef __INIT_PATCH__

  F_TRY(patches()->apply());

  current_ = this;

  return calibrate(this);
}

fat_bool_t PatchingInterceptor::uninstall() {
  current_ = NULL;
  return patches()->revert();
}

fat_bool_t PatchingInterceptor::calibrate(PatchingInterceptor *inter) {
  // Okay so this deserves a thorough explanation. What's going on here is that
  // when the connection between the process and the console server is first
  // opened the server describes itself and expects the client, that is the
  // functions in kernel32.dll, to express data in the server's terms. In
  // particular, pointers that are sent are relative to the server's view of
  // memory, not the client. The data we need is known by kernel32.dll but not
  // exposed so for us to be able to make sense of the messages we're going to
  // be intercepting we need to tease it out of kernel32 somehow. That. Is.
  // Tricky. And it's highly dependent on implementation details of kernel32
  // many of which change from version to version.
  //
  // This is the approach used for windows 7. Down the line we'll have to add
  // separate paths for other windows versions. It works as follows.
  //
  // The simplest function I've found that uses all the information we're
  // interested in is ConsoleClientCallServer. It takes a local message,
  // transforms it into a server message, and then sends it via LPC. So if we
  // can call it with data we control then we can intercept the LPC call and see
  // what it passed on and then the difference between the two tells us what we
  // need to know. Problem is, ConsoleClientCallServer isn't exposed so there's
  // no way to just call it. Instead what we do is call GetConsoleCP which we
  // know will call ConsoleClientCallServer and then in the LPC send function
  // which we can intercept. At that point a stack trace will contain pointers
  // into those functions, in particular the pc for GetConsoleCP will be
  // immediately after the instruction that called ConsoleClientCallServer.
  // That's how we get a hold of ConsoleClientCallServer. This is the best I've
  // been able to come up with. An alternative approach I've seen used is to
  // take GetConsoleCP and then disassemble it one instruction at a time until
  // you reach a call which is assumed to be ConsoleClientCallServer. This, to
  // me, seems even more fragile.

  // First try to locate ConsoleClientCallServer. The result variables start out
  // F_FALSE so if for some reason we don't hit the calibration handlers they'll
  // not be set to F_TRUE and we'll fail below.
  inter->one_shot_special_handler_ = inter->is_locating_cccs_ = true;
  GetConsoleCP();
  inter->is_locating_cccs_ = false;
  F_TRY(inter->locate_cccs_result_);

  // Then try determining the base port. We do this by intercepting a call
  // expected to go through that port.
  inter->one_shot_special_handler_ = inter->is_determining_base_port_ = true;
  dword_t dummy0 = 0;
  dword_t dummy1 = 0;
  GetProcessShutdownParameters(&dummy0, &dummy1);
  F_TRY(inter->determine_base_port_result_);

  // From here on we don't need to be in a static function so we let a method
  // do the rest.
  return inter->infer_calibration_from_cccs();
}

fat_bool_t PatchingInterceptor::infer_calibration_from_cccs() {
  return console_port()->infer_calibration(this);
}

fat_bool_t PatchingInterceptor::calibrate_console_port() {
  console_port_ = PortView();
  return infer_calibration_from_cccs();
}

fat_bool_t PortView::infer_calibration(PatchingInterceptor *inter) {
  // Then send the artificial calibration message.
  lpc::console_message_t message;
  struct_zero_fill(message);
  lpc::capture_buffer_data_t capbuf;
  struct_zero_fill(capbuf);

  inter->one_shot_special_handler_ = true;
  inter->active_calibration_ = this;
  (inter->cccs_)(&message, &capbuf,
      kCalibrationApiNumber, sizeof(message.payload.get_console_cp));
  inter->active_calibration_ = NULL;
  F_TRY(calibration_result_);

  // The calibration message appears to have gone well. Now we have the info we
  // need to calibration.
  address_t local_address = reinterpret_cast<address_t>(&capbuf);
  address_t remote_address = reinterpret_cast<address_t>(calibration_capbuf_);
  xform_ = AddressXform(local_address - remote_address);
  calibration_capbuf_ = NULL;

  return F_TRUE;
}

fat_bool_t PortView::process_calibration_message(handle_t port_handle,
    relevant_message_t *message) {
  calibration_capbuf_ = message->capture_buffer;
  handle_ = port_handle;
  calibration_result_ = F_TRUE;
  return F_TRUE;
}

static int32_t calc_stack_direction(size_t *caller_ptr) {
  size_t var = 0;
  return (caller_ptr == NULL)
      ? calc_stack_direction(&var)
      : ((&var < caller_ptr) ? -1 : 1);
}

int32_t PatchingInterceptor::infer_stack_direction() {
  return calc_stack_direction(NULL);
}

fat_bool_t PatchingInterceptor::process_locate_cccs_message(handle_t port_handle,
    Vector<void*> stack_trace) {
  locate_cccs_port_handle_ = port_handle;
  tclib::Blob gccp = tclib::Blob(GetConsoleCP, 256);
  // We always try to infer the address in two different ways because both are
  // somewhat unreliable so together they'll still be unreliable but should be
  // less so. First infer from a stack trace if we have one.
  void *guided_cccs = NULL;
  fat_bool_t guided_result;
  if (stack_trace.is_empty()) {
    guided_result = F_FALSE;
  } else {
    tclib::Blob expected_stack_blobs[kLocateCCCSStackTemplateSize] = {
        tclib::Blob(nt_request_wait_reply_port_bridge, 256),
        tclib::Blob(NULL /* ConsoleClientCallServer */, 256),
        gccp,
        tclib::Blob(calibrate, 256)
    };
    Vector<tclib::Blob> expected_stack = Vector<tclib::Blob>(expected_stack_blobs,
        kLocateCCCSStackTemplateSize);
    guided_result = infer_address_guided(expected_stack, stack_trace, &guided_cccs);
  }
  // Then infer from the body of GetConsoleCP. On 64 bit we can be a little
  // stricter and look for a unique 32-bit call because in practice there's
  // only one in the case we're interested in, whereas on 32 bits all calls are
  // like that so we have to make do with getting the first one.
  void *caller_cccs = NULL;
  fat_bool_t caller_result = infer_address_from_caller(gccp, &caller_cccs, kIs32Bit);
  if (guided_result) {
    if (caller_result && (guided_cccs != caller_cccs)) {
      // If both inferred an address but they landed at different ones that's
      // a bad sign so we abort. Most likely the guided one is correct but
      // still, better be on the safe side. Also, this provides some herd
      // immunity because it indicates that there's a problem in the caller
      // inference which is useful to know for platforms where the guided
      // inference isn't available.
      return F_FALSE;
    }
    cccs_ = reinterpret_cast<cccs_f>(guided_cccs);
    return F_TRUE;
  } else if (caller_result) {
    cccs_ = reinterpret_cast<cccs_f>(caller_cccs);
    return F_TRUE;
  } else {
    // If they both failed we can only return one so return this one.
    return caller_result;
  }
}

fat_bool_t PatchingInterceptor::process_base_request_message(handle_t port_handle,
    relevant_message_t *message) {
  base_port_handle_ = port_handle;
  return F_TRUE;
}

fat_bool_t Interceptor::capture_stacktrace(Vector<void*> buffer) {
  ulong_t frames = static_cast<ulong_t>(buffer.length());
  ushort_t captured = CaptureStackBackTrace(0, frames, buffer.start(), NULL);
  return F_BOOL(frames == captured);
}
