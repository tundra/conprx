//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/lpc.hh"
#include "sync/process.hh"
#include "utils/log.hh"

using namespace lpc;
using namespace tclib;
using namespace conprx;

PatchingInterceptor *PatchingInterceptor::current_ = NULL;

Interceptor::Interceptor()
  : enabled_(true) { }

Interceptor::Disable::Disable(Interceptor* interceptor)
  : interceptor_(interceptor)
  , was_enabled_(interceptor->enabled_) {
  interceptor->enabled_ = false;
}

Interceptor::Disable::~Disable() {
  interceptor_->enabled_ = was_enabled_;
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
  void *resolved = NativeProcess::resolve_jump_thunk(start);
  if (resolved == start)
    // The function was not a jump thunk so we're done.
    return false;
  // Grab the destination of the jump and try again. This will not terminate if
  // there is a cycle but why would there be?
  Blob new_fun(resolved, fun.size());
  return is_pc_within_function(pc, new_fun);
}

static bool extract_destination_from_call_pc(address_t call_pc, void **result_out) {
  // Check that it looks like a call. This may give false positives since it
  // might be a different instruction that happened to have an E8 in its
  // payload. But most likely it's not.
  if (call_pc[0] != 0xE8)
    return false;
  // Assuming it's a call the next 4 bytes will be the relative offset.
  int32_t call_offset = reinterpret_cast<int32_t*>(call_pc + 1)[0];
  // So this should be the address of the function we're looking for. The +5 is
  // again because it's relative to the end of the call instruction.
  *result_out = call_pc + 5 + call_offset;
  // Check that the pc we found above is within the candidate function. If the
  // above has gone wrong and it's not a call this is likely to catch it, but
  // it's not airtight.
  return true;
}

static bool extract_destination_from_return_pc(address_t caller_pc, void **result_out) {
  // Step backwards over the 32-bit relative call instruction we assume
  // will be here. We can in principle check that the functions aren't more
  // than 32 bits apart because we know both pcs but that can be added later if
  // necessary.
  return extract_destination_from_call_pc(caller_pc - 5, result_out);
}

fat_bool_t PatchingInterceptor::infer_address_guided(Vector<tclib::Blob> functions,
    Vector<void*> stack_trace, void **result_out) {
  size_t depth = functions.length();
  CHECK_REL("not enough stack", stack_trace.length(), >=, depth);
  size_t result_index = depth;
  // Peel off the top stack frames until we find one that matches the first
  // function.
  while (!stack_trace.is_empty()) {
    if (is_pc_within_function(stack_trace[0], functions[0]))
      break;
    stack_trace = Vector<void*>(stack_trace.start() + 1, stack_trace.length() - 1);
  }
  if (stack_trace.length() < depth)
    // There's not enough stack left to match the functions so we're done.
    return F_FALSE;
  // Scan through the stack trace and see if the entries lie within the
  // functions we expect them to.
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
      result_index = i;
      continue;
    }
    if (!is_pc_within_function(pc, fun))
      // We found a stack trace entry that's not in the function we expected;
      // bail.
      return F_FALSE;
  }
  CHECK_TRUE("no placeholder found", result_index != depth);
  CHECK_TRUE("placeholder at the bottom", result_index != (depth - 1));
  CHECK_TRUE("placeholder at the top", result_index != 0);
  // Okay so the stack has checked out so now we try to extract the address. We
  // first grab the pc of the function that called the one we're looking for.
  address_t caller_pc = reinterpret_cast<address_t>(stack_trace[result_index + 1]);
  void *result = NULL;
  if (!extract_destination_from_return_pc(caller_pc, &result))
    return F_FALSE;
  // Check that the pc we found above is within the candidate function. If the
  // above has gone wrong and it's not a call this is likely to catch it, but
  // it's not airtight.
  void *result_pc = stack_trace[result_index];
  size_t result_size = functions[result_index].size();
  if (!is_pc_within_function(result_pc, tclib::Blob(result, result_size)))
    return F_FALSE;
  *result_out = result;
  return F_TRUE;
}

fat_bool_t PatchingInterceptor::infer_address_from_caller(tclib::Blob function,
    void **result_out, bool return_first) {
  address_t ptr = static_cast<address_t>(function.start());
  fat_bool_t result_on_ret = F_FALSE;
  bool seen_result = false;
  for (size_t i = 0; i < function.size(); i++) {
    if (ptr[i] == 0xC3)
      // Return -- we can't scan on past this point so if we haven't found it
      // we're not going to.
      return result_on_ret;
    if (ptr[i] == 0xE8) {
      void *dest = NULL;
      if (extract_destination_from_call_pc(ptr + i, &dest)) {
        if (seen_result) {
          // This is the second plausible call we've seen and it's impossible
          // to know which is the right one. So we have to give up.
          return F_FALSE;
        } else {
          *result_out = dest;
          if (return_first) {
            return F_TRUE;
          } else {
            // This is the first match we've seen, hopefully the only. Record it
            // and keep going.
            seen_result = true;
            result_on_ret = F_TRUE;
          }
        }
      }
    }
  }
  // We never saw a return so whether we found a result or not, if we haven't
  // seen the end of the function we don't know if the result is unique so we
  // have to bail.
  return F_FALSE;
}

Message::Message(message_data_t *request, message_data_t *reply,
    Interceptor *interceptor, AddressXform xform)
  : request_(request)
  , reply_(reply)
  , interceptor_(interceptor)
  , xform_(xform) {
}

void Message::set_return_value(NtStatus status) {
  reply_->header.return_value = status.to_nt();
}

void Message::dump(OutStream *out, Blob::DumpStyle style) {
  size_t total_size = this->total_length();
  size_t data_size = this->data_length();
  size_t header_size = total_size - data_size;
  out->printf("--- message %x [size: %i, data: %i]\n", api_number(), total_size, data_size);
  address_t start = reinterpret_cast<address_t>(request_);
  tclib::Blob header(start, header_size);
  header.dump(out, style);
  out->printf("\n--- data ---\n");
  tclib::Blob data(start + header_size, data_size);
  data.dump(out, style);
  out->printf("\n");
  out->flush();
}

NtStatus Message::call_native_backend() {
  return interceptor()->call_native_backend(request_, reply_);
}

#ifdef IS_MSVC
#  include "lpc-msvc.cc"
#else
#  include "lpc-gnu.cc"
#endif
