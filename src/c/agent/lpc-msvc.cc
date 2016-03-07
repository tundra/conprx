//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/lpc.hh"
#include "binpatch.hh"
#include "utils/log.hh"

using namespace conprx;
using namespace lpc;
using namespace tclib;

Interceptor::Interceptor(handler_t handler)
  : handler_(handler)
  , patches_(Platform::get(), Vector<PatchRequest>(patch_requests_, kPatchRequestCount))
  , enabled_(true)
  , enable_special_handlers_(false)
  , is_locating_cccs_(false)
  , locate_cccs_result_(F_FALSE)
  , console_client_call_server_(NULL)
  , locate_cccs_port_handle_(INVALID_HANDLE_VALUE)
  , is_calibrating_(false)
  , calibrate_result_(F_FALSE)
  , calibration_capture_buffer_(NULL)
  , console_server_port_handle_(INVALID_HANDLE_VALUE) { }

ntstatus_t NTAPI Interceptor::nt_request_wait_reply_port_bridge(handle_t port_handle,
    lpc::message_data_t *request, lpc::message_data_t *incoming_reply) {
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
  Interceptor *inter = current();
  if (inter->enable_special_handlers_) {
    if (request->api_number == kGetConsoleCPApiNumber && inter->is_locating_cccs_) {
      void *stack[kLocateCCCSStackSize];
      memset(stack, 0, kLocateCCCSStackSize * sizeof(void*));
      dword_t captured = CaptureStackBackTrace(0, kLocateCCCSStackSize, stack, NULL);
      if (captured < kLocateCCCSStackSize) {
        inter->locate_cccs_result_ = F_FALSE;
        return 0;
      } else {
        inter->locate_cccs_result_ = inter->process_locate_cccs_message(port_handle, stack);
        return 0;
      }
    } else if (request->api_number == kCalibrationApiNumber && inter->is_calibrating_) {
      inter->calibrate_result_ = inter->process_calibration_message(port_handle, request);
      return 0;
    } else {
      return inter->nt_request_wait_reply_port_imposter(port_handle, request, incoming_reply);
    }
  } else {
    return inter->nt_request_wait_reply_port(port_handle, request, incoming_reply);
  }
}

ntstatus_t Interceptor::nt_request_wait_reply_port(handle_t port_handle,
    message_data_t *request, message_data_t *incoming_reply) {
  if (enabled_ && port_handle == console_server_port_handle_) {
    lpc::Message message(request, port_xform());
    fat_bool_t result = handler_(this, &message, incoming_reply);
    return result ? 0 : 1;
  } else {
    return nt_request_wait_reply_port_imposter(port_handle, request, incoming_reply);
  }
}

fat_bool_t Interceptor::initialize_patch(PatchRequest *request,
    const char *name, address_t replacement, module_t ntdll) {
  address_t original = Code::upcast(GetProcAddress(ntdll, name));
  if (original == NULL) {
    WARN("GetProcAddress(-, \"%s\"): %i", name, GetLastError());
    return F_FALSE;
  }
  *request = PatchRequest(original, replacement);
  return F_TRUE;
}

fat_bool_t Interceptor::install() {
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

fat_bool_t Interceptor::uninstall() {
  current_ = NULL;
  return patches()->revert();
}

fat_bool_t Interceptor::calibrate(Interceptor *inter) {
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
  inter->enable_special_handlers_ = inter->is_locating_cccs_ = true;
  GetConsoleCP();
  inter->enable_special_handlers_ = inter->is_locating_cccs_ = false;
  F_TRY(inter->locate_cccs_result_);

  // From here on we don't need to be in a static function so we let a method
  // do the rest.
  return inter->infer_calibration_from_cccs();
}

fat_bool_t Interceptor::infer_calibration_from_cccs() {
  // Then send the artificial calibration message.
  message_data_t message;
  struct_zero_fill(message);
  capture_buffer_data_t capture_buffer;
  struct_zero_fill(capture_buffer);
  enable_special_handlers_ = is_calibrating_ = true;
  (console_client_call_server_)(&message, &capture_buffer, kCalibrationApiNumber,
      0);
  enable_special_handlers_ = is_calibrating_ = false;
  F_TRY(calibrate_result_);

  // The calibration message appears to have gone well. Now we have the info we
  // need to calibration.
  address_t local_address = reinterpret_cast<address_t>(&capture_buffer);
  address_t remote_address = reinterpret_cast<address_t>(calibration_capture_buffer_);
  port_xform()->initialize(local_address - remote_address);
  console_server_port_handle_ = locate_cccs_port_handle_;

  return F_TRUE;
}

// Returns true iff the given pc is within the function covered by the given
// blob *or* we can determine that the blob is actually equivalent to some other
// function and the pc is within that.
static bool is_pc_within_function(void *pc, tclib::Blob fun) {
  void *start = fun.start();
  void *end = fun.end();
  if (start <= pc && pc <= end)
    // The pc is trivially within the function.
    return true;
  // If it's not within the function directly it might be because that function
  // is simply a jump to another function, this is for instance the case with
  // incremental linking. So we check if it is.
  address_t addr = reinterpret_cast<address_t>(start);
  if (addr[0] != 0xE9)
    // First instruction is not a jump so we're done.
    return false;
  // Grab the destination of the jump and try again. This will not terminate if
  // there is a cycle but why would there be?
  int32_t offset = reinterpret_cast<int32_t*>(addr + 1)[0];
  void *new_start = addr + 5 + offset;
  Blob new_fun(new_start, fun.size());
  return is_pc_within_function(pc, new_fun);
}

// Try to infer the address of a function given an array of function pointers
// that we expect will be present in a stack trace, as well as a stack trace.
// The function array is expected to contain one empty blob which indicates the
// position of the function to infer.
fat_bool_t Interceptor::infer_address_guided(tclib::Blob *functions, void **stack_trace,
    size_t depth, void **result_out) {
  size_t placeholder_index = depth;
  // Scan through the stack trace and see if the entries lie within the
  // functions we expect them to.
  void *result_pc = NULL;
  size_t result_size = 0;
  for (size_t i = 0; i < depth; i++) {
    tclib::Blob fun = functions[i];
    void *pc = stack_trace[i];
    if (pc == NULL)
      // The stack trace function sometimes leaves null entries, or doesn't
      // fill them out. In either case it's a sign this won't work.
      return F_FALSE;
    if (fun.is_empty()) {
      // If this is the placeholder we record its index as well as the pc which
      // we can assume will be within the function.
      placeholder_index = i;
      result_pc = pc;
      result_size = fun.size();
      continue;
    }
    if (!is_pc_within_function(pc, fun))
      // We found a stack trace entry that's not in the function we expected;
      // bail.
      return F_FALSE;
  }
  CHECK_TRUE("no placeholder found", placeholder_index != depth);
  CHECK_TRUE("placeholder at the bottom", placeholder_index != (depth - 1));
  // Okay so the stack has checked out so now we try to extract the address. We
  // first grab the pc of the function that called the one we're looking for.
  address_t caller_pc = reinterpret_cast<address_t>(stack_trace[placeholder_index + 1]);
  // Then we step backwards over the 32-bit relative call instruction we assume
  // will be there. We can in principle check that the functions aren't more
  // than 32 bits apart because we know both pcs but that can be added later if
  // necessary.
  address_t call_pc = caller_pc - 5;
  // Check that it looks like a call. This may give false positives since it
  // might be a different instruction that happened to have an E8 in its
  // payload. But most likely it's not.
  if (call_pc[0] != 0xE8)
    return F_FALSE;
  // Assuming it's a call the next 4 bytes will be the relative offset.
  int32_t call_offset = reinterpret_cast<int32_t*>(call_pc + 1)[0];
  // So this should be the address of the function we're looking for. The +5 is
  // again because it's relative to the end of the call instruction.
  void *result = call_pc + 5 + call_offset;
  // Check that the pc we found above is within the candidate function. If the
  // above has gone wrong and it's not a call this is likely to catch it, but
  // it's not airtight.
  if (!is_pc_within_function(result_pc, tclib::Blob(result, result_size)))
    return F_FALSE;
  *result_out = result;
  return F_TRUE;
}

fat_bool_t Interceptor::process_locate_cccs_message(handle_t port_handle,
    void **stack) {
  locate_cccs_port_handle_ = port_handle;
  tclib::Blob expected_stack[4] = {
      tclib::Blob(nt_request_wait_reply_port_bridge, 256),
      tclib::Blob(NULL, 256),
      tclib::Blob(GetConsoleCP, 256),
      tclib::Blob(calibrate, 256)
  };
  void *cccs = NULL;
  F_TRY(infer_address_guided(expected_stack, stack, 4, &cccs));
  console_client_call_server_ = reinterpret_cast<console_client_call_server_f>(cccs);
  return F_TRUE;
}

fat_bool_t Interceptor::process_calibration_message(handle_t port_handle,
    message_data_t *message) {
  if (locate_cccs_port_handle_ != port_handle)
    // Definitely both of these messages should be going to the same port so
    // if they're not something is not right and we abort.
    return F_FALSE;
  calibration_capture_buffer_ = message->capture_buffer;
  return F_TRUE;
}
