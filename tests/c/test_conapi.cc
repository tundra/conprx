//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "agent/conapi.hh"

using namespace conprx;

TEST(conapi, datatypes) {
  ASSERT_EQ(sizeof(long), sizeof(dword_t));
  ASSERT_EQ(sizeof(void*), sizeof(handle_t));
}

template <typename T>
void use(T t) { }

TEST(conapi, functypes) {
#ifdef IS_MSVC
#define __EMIT_ASSIGN__(Name, name, RET, PARAMS, ARGS)                         \
  Console::name##_t name = Name;                                               \
  use(name);
  FOR_EACH_CONAPI_FUNCTION(__EMIT_ASSIGN__)
#undef __EMIT_ASSIGN__
#endif
}
