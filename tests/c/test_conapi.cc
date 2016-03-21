//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/confront.hh"
#include "test/asserts.hh"
#include "test/unittest.hh"

using namespace conprx;

TEST(conapi, datatypes) {
  ASSERT_EQ(sizeof(uint32_t), sizeof(dword_t));
  ASSERT_EQ(sizeof(void*), sizeof(handle_t));
}

template <typename T>
void use(T t) { }

TEST(conapi, functypes) {
#ifdef IS_MSVC
#  define __EMIT_ASSIGN__(Name, name, FLAGS, SIG, PSIG)                        \
   ConsoleFrontend::name##_t name = Name;                                      \
   use(name);
   FOR_EACH_CONAPI_FUNCTION(__EMIT_ASSIGN__)
#  undef __EMIT_ASSIGN__
#else
  SKIP_TEST("msvc only");
#endif
}
