//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/binpatch.hh"
#include "disass/disassembler-x86.hh"
#include "test.hh"
#include "utils/macro-inl.h"

using namespace conprx;

// Helpers for running snippet tests.
class SnippetHelper {
public:
  typedef void *snippet_t;
  typedef int32_t (function_t)(int32_t, int32_t);

  // Finds the asm snippet contained in the code of f.
  static snippet_t find_snippet(function_t f);

  // Invoke the given snippet which was extracted using find_snippet.
  static int32_t call_raw_snippet(int32_t a, int32_t b, snippet_t snippet);

  // Extracts and calls the snippet contained in the code for f.
  static int32_t call_snippet(function_t f, int32_t a, int32_t b);

  static Redirection::Type test_patch(int32_t e, function_t f, int32_t a,
      int32_t b, int32_t flags = 0);

  static int32_t intercept(int32_t a, int32_t b);

private:
  static const int kSnippetMarkerConstant = 0xDEC0DED;
};

int32_t SnippetHelper::call_raw_snippet(int32_t a, int32_t b, snippet_t s) {
  int32_t result;
  asm volatile(
      "call *%%rdx\n\t"
      : "=a"(result)
      : "a"(a), "b"(b), "d"(s));
  return result;
}

SnippetHelper::snippet_t SnippetHelper::find_snippet(function_t f) {
  uint8_t *ptr = reinterpret_cast<uint8_t*>(f);
  for (int i = 0; i < 1024; i++) {
    int32_t word = *reinterpret_cast<int32_t*>(ptr + i);
    if (word == kSnippetMarkerConstant)
      return ptr + i + sizeof(int32_t);
  }
  ASSERT_TRUE(false);
  return NULL;
}

int32_t SnippetHelper::call_snippet(function_t f, int32_t a, int32_t b) {
  snippet_t s = find_snippet(f);
  return call_raw_snippet(a, b, s);
}

#define BEGIN_SNIPPET asm volatile("push 0xDEC0DED\n\t"
#define END_SNIPPET ); return 0;
#define NOP(A, B) "nop\n\t"
#define NOPS(...) FOR_EACH_VA_ARG(NOP, _, __VA_ARGS__)

static int32_t add_short(int32_t a, int32_t b) {
  BEGIN_SNIPPET
    "add %ebx,%eax\n\t" // 2
    NOPS(,,,,,,,,,)     // 10
    "ret\n\t"           // 1
  END_SNIPPET
}

int32_t SnippetHelper::intercept(int32_t a, int32_t b) {
  return 79;
}

static int32_t add_really_short(int32_t a, int32_t b) {
  BEGIN_SNIPPET
    "add %ebx,%eax\n\t" // 2
    NOPS(,,)            // 3
    "ret\n\t"           // 1
  END_SNIPPET
}

Redirection::Type SnippetHelper::test_patch(int32_t e, function_t f, int32_t a,
    int32_t b, int32_t flags) {
  snippet_t snip = find_snippet(f);
  ASSERT_EQ(e, call_raw_snippet(a, b, snip));
  PatchRequest req(reinterpret_cast<address_t>(snip), reinterpret_cast<address_t>(intercept));
  req.set_flags(flags);
  PatchSet set(Platform::get(), Vector<PatchRequest>(&req, 1));
  ASSERT_F_TRUE(set.apply());
  ASSERT_EQ(79, call_raw_snippet(a, b, snip));
  Redirection::Type type = req.redirection_type();
  ASSERT_F_TRUE(set.revert());
  ASSERT_EQ(e, call_raw_snippet(a, b, snip));
  return type;
}

TEST(binpatch, add_short) {
  Redirection::Type long_redir = kIs32Bit ? Redirection::rtRel32 : Redirection::rtAbs64;
  ASSERT_EQ(long_redir, SnippetHelper::test_patch(17, add_short, 8, 9));
  Redirection::Type short_redir = Redirection::rtRel32;
  ASSERT_EQ(short_redir, SnippetHelper::test_patch(19, add_really_short, 9, 10));
  if (kIs64Bit) {
    ASSERT_EQ(Redirection::rtKangaroo,
        SnippetHelper::test_patch(19, add_really_short, 9, 10,
            PatchRequest::pfBanRel32));
  }
}
