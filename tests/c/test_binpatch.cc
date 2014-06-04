//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Code patching.

#include "agent/binpatch.hh"
#include "test.hh"

TEST(binpatch, datatypes) {
  ASSERT_EQ(1, sizeof(byte_t));
}

int add_impl(int a, int b) {
  return a + b;
}

// Alias for add which hopefully eager compilers won't inline, unlike add_impl.
int (*add)(int, int) = add_impl;

int new_add(int a, int b) {
  return a + b + 1;
}

TEST(binpatch, individual_steps) {
  Platform &platform = Platform::get();
  ASSERT_TRUE(platform.ensure_initialized());

  ASSERT_EQ(8, add(3, 5));
  PatchRequest patch(Code::upcast(add), Code::upcast(new_add));
  PatchSet patches(platform, Vector<PatchRequest>(&patch, 1));

  ASSERT_EQ(PatchSet::NOT_APPLIED, patches.status());
  ASSERT_TRUE(patches.prepare_apply());
  ASSERT_EQ(PatchSet::PREPARED, patches.status());

  ASSERT_TRUE(patches.open_for_patching());
  ASSERT_EQ(PatchSet::OPEN, patches.status());

  patches.install_redirects();
  ASSERT_EQ(PatchSet::APPLIED_OPEN, patches.status());

  ASSERT_TRUE(patches.close_after_patching(PatchSet::APPLIED));
  ASSERT_EQ(PatchSet::APPLIED, patches.status());

  ASSERT_EQ(9, add(3, 5));
  // ASSERT_EQ(8, patch.get_trampoline(add)(3, 5));

  ASSERT_TRUE(patches.open_for_patching());
  ASSERT_EQ(PatchSet::OPEN, patches.status());

  patches.revert_redirects();
  ASSERT_EQ(PatchSet::REVERTED_OPEN, patches.status());

  ASSERT_TRUE(patches.close_after_patching(PatchSet::NOT_APPLIED));
  ASSERT_EQ(PatchSet::NOT_APPLIED, patches.status());

  ASSERT_EQ(8, add(3, 5));
}

TEST(binpatch, address_range) {
  Platform &platform = Platform::get();
  ASSERT_TRUE(platform.ensure_initialized());

  PatchSet p0(platform, Vector<PatchRequest>());
  Vector<byte_t> r0 = p0.determine_address_range();
  ASSERT_PTREQ(NULL, r0.start());

  PatchRequest p1s[1] = {
    PatchRequest(CODE_UPCAST(7), NULL),
  };
  PatchSet p1(platform, Vector<PatchRequest>(p1s, 1));
  Vector<byte_t> r1 = p1.determine_address_range();
  ASSERT_PTREQ(reinterpret_cast<address_t>(7), r1.start());
  ASSERT_PTREQ(reinterpret_cast<address_t>(8), r1.end());

  PatchRequest p3s[3] = {
    PatchRequest(CODE_UPCAST(15), NULL),
    PatchRequest(CODE_UPCAST(10), NULL),
    PatchRequest(CODE_UPCAST(36), NULL)
  };
  PatchSet p3(platform, Vector<PatchRequest>(p3s, 3));
  Vector<byte_t> r3 = p3.determine_address_range();
  ASSERT_PTREQ(reinterpret_cast<address_t>(10), r3.start());
  ASSERT_PTREQ(reinterpret_cast<address_t>(37), r3.end());
}

TEST(binpatch, fits_in_32_bits) {
  ASSERT_TRUE(PatchSet::offsets_fit_in_32_bits(0));
  ASSERT_TRUE(PatchSet::offsets_fit_in_32_bits(65536));
  ASSERT_TRUE(PatchSet::offsets_fit_in_32_bits(0x7FFFFFFE));
  ASSERT_TRUE(PatchSet::offsets_fit_in_32_bits(0x7FFFFFFF));
  ASSERT_FALSE(PatchSet::offsets_fit_in_32_bits(0x80000000));
  ASSERT_FALSE(PatchSet::offsets_fit_in_32_bits(0x80000001));
}

TEST(binpatch, casts) {
  address_t add_addr = CODE_UPCAST(add_impl);
  int (*my_add)(int, int) = reinterpret_cast<int(*)(int, int)>(add_addr);
  ASSERT_TRUE(my_add == add_impl);
  ASSERT_EQ(7, my_add(3, 4));
  int (*my_add_2)(int, int) = Code::downcast(add_impl, add_addr);
  ASSERT_TRUE(my_add_2 == add_impl);
  ASSERT_EQ(7, my_add_2(3, 4));
  address_t add_addr_2 = Code::upcast(add_impl);
  ASSERT_PTREQ(add_addr, add_addr_2);
}
