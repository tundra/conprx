//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"
#include "agent/lpc.hh"

using namespace lpc;

int fun_three(Vector<void*> trace, Vector<void*> top, fat_bool_t *trace_result) {
  *trace_result = Interceptor::capture_stacktrace(trace);
  Interceptor::capture_stack_top(top);
  return 4;
}

int fun_two(Vector<void*> trace, Vector<void*> top, fat_bool_t *capture_result) {
  return fun_three(trace, top, capture_result) + 2;
}

int fun_one(Vector<void*> trace, Vector<void*> top, fat_bool_t *capture_result) {
  return fun_two(trace, top, capture_result) + 1;
}

TEST(lpc, infer_unguided_successful) {
  void *trace_scratch[4];
  struct_zero_fill(trace_scratch);
  Vector<void*> stack(trace_scratch, 4);

  void *top_scratch[64];
  struct_zero_fill(top_scratch);
  Vector<void*> top(top_scratch, 64);

  fat_bool_t capture_result = F_FALSE;
  ASSERT_EQ(7, fun_one(stack, top, &capture_result));
  tclib::Blob fun_array[3] = {
    tclib::Blob(CODE_UPCAST(fun_three), 128),
    tclib::Blob(NULL, 128),
    tclib::Blob(CODE_UPCAST(fun_one), 128)
  };
  Vector<tclib::Blob> funs(fun_array, 3);

  if (kIsDebugCodegen)
    // With debug codegen there is no excuse for this not to work.
    ASSERT_F_TRUE(capture_result);
  if (!capture_result)
    return;
  void *guided_out = NULL;
  fat_bool_t guided_result = Interceptor::infer_address_guided(funs, stack, &guided_out);
  if (kIsDebugCodegen)
    ASSERT_F_TRUE(guided_result);
  if (guided_result)
    ASSERT_PTREQ(CODE_UPCAST(fun_two), guided_out);

  void *unguided_out = NULL;
  fat_bool_t unguided_result = Interceptor::infer_address_unguided(funs, top,
      &unguided_out, Vector<void*>());
  if (kIsDebugCodegen)
    ASSERT_F_TRUE(unguided_result);
  if (unguided_result)
    ASSERT_PTREQ(CODE_UPCAST(fun_two), unguided_out);
}
