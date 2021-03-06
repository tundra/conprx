//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/confront.hh"
#include "test/asserts.hh"
#include "test/unittest.hh"

using namespace conprx;
using namespace tclib;

TEST(conapi, datatypes) {
  ASSERT_EQ(sizeof(uint32_t), sizeof(dword_t));
  ASSERT_EQ(sizeof(void*), sizeof(handle_t));
}

TEST(conapi, platform) {
  def_ref_t<ConsolePlatform> platform = InMemoryConsolePlatform::new_simulating(NULL);
  Handle stdin_handle = platform->get_std_handle(kStdInputHandle);
  ASSERT_TRUE(stdin_handle.is_console());
  Handle stdout_handle = platform->get_std_handle(kStdOutputHandle);
  ASSERT_TRUE(stdout_handle.is_console());
  Handle stderr_handle = platform->get_std_handle(kStdErrorHandle);
  ASSERT_TRUE(stderr_handle.is_console());
  ASSERT_FALSE(Handle(platform->get_std_handle(0)).is_valid());
}

template <typename T>
void use(T t) { }

TEST(conapi, functypes) {
#ifdef IS_MSVC
#  define __EMIT_ASSIGN__(Name, name, FLAGS, SIG, PSIG)                        \
   ConsoleFrontend::name##_t name = Name;                                      \
   use(name);
#  define __MAYBE_EMIT_ASSIGN__(Name, name, FLAGS, SIG, PSIG) mfSt FLAGS (, __EMIT_ASSIGN__(Name, name, FLAGS, SIG, PSIG))
   FOR_EACH_CONAPI_FUNCTION(__MAYBE_EMIT_ASSIGN__)
#  undef __EMIT_ASSIGN__
#  undef __MAYBE_EMIT_ASSIGN__
#else
  SKIP_TEST("msvc only");
#endif
}
