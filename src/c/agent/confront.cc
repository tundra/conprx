//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Shared console api implementation.

#include "confront.hh"

#include "utils/string.hh"
#include "plankton-inl.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;

ConsoleFrontend::~ConsoleFrontend() { }

static ConsoleFrontend::FunctionInfo function_info[ConsoleFrontend::kFunctionCount + 1] = {
#define __DECLARE_INFO__(Name, name, ...) {TEXT(#Name), ConsoleFrontend::name##_key} ,
    FOR_EACH_CONAPI_FUNCTION(__DECLARE_INFO__)
#undef __DECLARE_INFO__
    {NULL, 0}
};

Vector<ConsoleFrontend::FunctionInfo> ConsoleFrontend::functions() {
  return Vector<FunctionInfo>(function_info, ConsoleFrontend::kFunctionCount);
}

// --- L o g g i n g ---

// A plankton arena with some extra utilities for console api logging.
class log_arena_t : public Arena {
public:
  // Allocate a new arena string with the same contents as the given utf16
  // string.
  Variant new_utf16(const void *raw_utf16, size_t length);

  // Returns a coordinate variant corresponding to the given value.
  Variant new_coord(const coord_t &coord);

  // Returns a small rect variant corresponding to the given value.
  Variant new_small_rect(const small_rect_t &small_rect);
};

Variant log_arena_t::new_utf16(const void *raw_utf16, size_t utf16_length) {
  wide_cstr_t utf16 = static_cast<wide_cstr_t>(raw_utf16);
  char *utf8 = NULL;
  size_t utf8_length = conprx::String::utf16_to_utf8(utf16, utf16_length, &utf8);
  Variant result = new_string(utf8, (uint32_t) utf8_length);
  delete[] utf8;
  return result;
}

Variant log_arena_t::new_coord(const coord_t &coord) {
  Map map = new_map();
  map.set("x", coord.X);
  map.set("y", coord.Y);
  return map;
}

Variant log_arena_t::new_small_rect(const small_rect_t &small_rect) {
  Map map = new_map();
  map.set("left", small_rect.Left);
  map.set("top", small_rect.Top);
  map.set("right", small_rect.Right);
  map.set("bottom", small_rect.Bottom);
  return map;
}

static Variant handle_variant(handle_t handle) {
  return Variant::integer(reinterpret_cast<int64_t>(handle));
}

#include "confront-dummy.cc"

#include "confront-simul.cc"

#ifdef IS_MSVC
#  include "confront-msvc.cc"
#endif
