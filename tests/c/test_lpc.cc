//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"
#include "agent/lpc.hh"

using namespace lpc;

int fun_three(Vector<void*> trace, fat_bool_t *trace_result) {
  *trace_result = Interceptor::capture_stacktrace(trace);
  return 4;
}

int fun_two(Vector<void*> trace, fat_bool_t *capture_result) {
  return fun_three(trace, capture_result) + 2;
}

int fun_one(Vector<void*> trace, fat_bool_t *capture_result) {
  return fun_two(trace, capture_result) + 1;
}

TEST(lpc, infer_unguided_successful) {
  void *trace_scratch[4];
  struct_zero_fill(trace_scratch);
  Vector<void*> stack(trace_scratch, 4);

  fat_bool_t capture_result = F_FALSE;
  ASSERT_EQ(7, fun_one(stack, &capture_result));
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
}

#define FOFF(FIELD) offsetof(lpc::message_data_t, payload.FIELD)

TEST(lpc, offsets) {
  if (!kIsMsvc)
    SKIP_TEST("msvc only");
  // Various offsets of the different data structures that have been known to
  // work. These failing is a convenient way to catch the struct packing getting
  // messed up, rather than just seeing operations subtly failing.
  ASSERT_EQ(IF_32_BIT(40, 64), FOFF(get_console_cp));
  ASSERT_EQ(IF_32_BIT(44, 68), FOFF(get_console_cp.is_output));
  ASSERT_EQ(IF_32_BIT(44, 72), FOFF(set_console_title.title));
}
