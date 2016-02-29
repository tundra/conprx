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
  , is_locating_cccs_(false)
  , locate_cccs_result_(F_FALSE)
  , console_client_call_server_(NULL)
  , locate_cccs_port_handle_(INVALID_HANDLE_VALUE)
  , is_calibrating_(false)
  , calibrate_result_(F_FALSE)
  , calibration_capture_buffer_(NULL)
  , console_server_port_handle_(INVALID_HANDLE_VALUE) { }

#define __DEFINE_LPC_BRIDGE__(Name, name, RET, SIG, ARGS)                      \
  RET NTAPI Interceptor::name##_bridge SIG {                                   \
    return current()->name ARGS;                                               \
  }
FOR_EACH_LPC_FUNCTION(__DEFINE_LPC_BRIDGE__)
#undef __DEFINE_LPC_BRIDGE__

ntstatus_t Interceptor::nt_request_wait_reply_port(handle_t port_handle,
    message_data_t *request, message_data_t *incoming_reply) {
  if (enabled_) {
    if (request->api_number == kGetConsoleCPApiNumber && is_locating_cccs_) {
      // If this call is used to locate ConsoleClientCallServer
      void *stack[kLocateCCCSStackSize];
      CaptureStackBackTrace(0, kLocateCCCSStackSize, stack, NULL);
      locate_cccs_result_ = process_locate_cccs_message(port_handle, stack);
      return 0;
    } else if (request->api_number == kCalibrationApiNumber && is_calibrating_) {
      calibrate_result_ = process_calibration_message(port_handle, request);
      return 0;
    } else if (port_handle == console_server_port_handle_) {
      lpc::Message message(request, port_xform());
      fat_bool_t result = handler_(this, &message, incoming_reply);
      return result ? 0 : 1;
    }
  }
  return nt_request_wait_reply_port_imposter(port_handle, request, incoming_reply);
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

fat_bool_t Interceptor::calibrate(Interceptor *interceptor) {
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
  interceptor->is_locating_cccs_ = true;
  GetConsoleCP();
  interceptor->is_locating_cccs_ = false;
  F_TRY(interceptor->locate_cccs_result_);

  // From here on we don't need to be in a static function so we let a method
  // do the rest.
  return interceptor->infer_calibration_from_cccs();
}

fat_bool_t Interceptor::infer_calibration_from_cccs() {
  // Then send the artificial calibration message.
  message_data_t message;
  struct_zero_fill(message);
  capture_buffer_data_t capture_buffer;
  struct_zero_fill(capture_buffer);
  is_calibrating_ = true;
  (console_client_call_server_)(&message, &capture_buffer, kCalibrationApiNumber,
      0);
  is_calibrating_ = false;
  F_TRY(calibrate_result_);

  // The calibration message appears to have gone well. Now we have the info we
  // need to calibration.
  address_t local_address = reinterpret_cast<address_t>(&capture_buffer);
  address_t remote_address = reinterpret_cast<address_t>(calibration_capture_buffer_);
  port_xform()->initialize(local_address - remote_address);
  console_server_port_handle_ = locate_cccs_port_handle_;

  return F_TRUE;
}

// Given a pointer within a function and the function, returns how far inside
// the function the pc is.
static size_t get_pc_offset(void *pc, void *func) {
  return reinterpret_cast<address_t>(pc) - reinterpret_cast<address_t>(func);
}

// Given a function pointer return the function it *really* represents, in the
// very limited sense that if the given function is just a jump to another
// function, which if incremental linking is enabled it might be, then that
// other function is what this will return.
static address_t get_effective_function_address(void *raw_addr) {
  address_t addr = reinterpret_cast<address_t>(raw_addr);
  if (addr[0] != 0xE9)
    // First instruction is not a jump so we're done.
    return addr;
  // Grab the destination of the jump and try again. This will not terminate if
  // there is a cycle but why would there be?
  int32_t offset = reinterpret_cast<int32_t*>(addr + 1)[0];
  return get_effective_function_address(addr + 5 + offset);
}

fat_bool_t Interceptor::process_locate_cccs_message(handle_t port_handle,
    void **stack) {
  // Save the port handle for later, we'll need it during calibration.
  locate_cccs_port_handle_ = port_handle;
  // Check the assumption that the immediate caller is the bridge function.
  void *ntrwrpb_pc = stack[1];
  address_t ntrwrpb = get_effective_function_address(nt_request_wait_reply_port_bridge);
  size_t ntrwrpb_offset = get_pc_offset(ntrwrpb_pc, ntrwrpb);
  if (ntrwrpb_offset > 256)
    return F_FALSE;
  // If we're right this will be an offset within ConsoleClientCallServer.
  void *cccs_pc = stack[2];
  // Check that the caller of ConsoleClientCallServer is GetConsoleCP.
  address_t gcc_pc = reinterpret_cast<address_t>(stack[3]);
  address_t gcc = get_effective_function_address(GetConsoleCP);
  size_t gcc_offset =  get_pc_offset(gcc_pc, gcc);
  if (gcc_offset >= 256)
    return F_FALSE;
  // Check that the caller of GetConsoleCP is calibrate. This is why calibrate
  // needs to be static at least until the point where it calls GetConsoleCP.
  void *crv_pc = stack[4];
  address_t crv = get_effective_function_address(calibrate);
  size_t crv_offset = get_pc_offset(crv_pc, crv);
  if (crv_offset >= 256)
    return F_FALSE;

  // Okay, we've verified that the stack looks roughly as expected. Now we'll
  // try to extract the address of ConsoleClientCallServer from GetConsoleCP.
  // First, we assume we're standing immediately after a 32-bit relative CALL
  // since both functions live in the same DLL. So we first step back to before
  // the assumed call.
  address_t cccs_call_pc = gcc_pc - 5;
  // Check that it looks like a call. This may give false positives since it
  // might be a different instruction that happened to have an E8 in its
  // payload. But most likely it's not.
  if (cccs_call_pc[0] != 0xE8)
    return F_FALSE;
  // Assuming it's a call the next 4 bytes will be the relative offset.
  int32_t call_offset = reinterpret_cast<int32_t*>(cccs_call_pc + 1)[0];
  // So this should be the address of the function we're looking for. The +5 is
  // because it's relative to the end of the call instruction.
  address_t cccs = cccs_call_pc + 5 + call_offset;
  // Check that the pc we found above is within the candidate function. If the
  // above has gone wrong and it's not a call this is likely to catch it, but
  // it's not airtight.
  size_t cccs_offset = get_pc_offset(cccs_pc, cccs);
  if (cccs_offset >= 256)
    return F_FALSE;
  // Okay, to the best of our knowledge this is the right function. Store it and
  // don't look back.
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
