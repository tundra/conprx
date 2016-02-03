//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Code patching.

#include "agent/binpatch.hh"
#include "disass/disassembler-x86.hh"
#include "agent/binpatch-x86.hh"
#include "test.hh"

using namespace conprx;

TEST(binpatch, datatypes) {
  ASSERT_EQ(1, sizeof(byte_t));
}

int add_impl(int a, int b) {
  return a + b + 1;
}

// Alias for add which hopefully eager compilers won't inline, unlike add_impl.
int (*add)(int, int) = add_impl;
int new_extra_incr = 2;

int new_add(int a, int b) {
  return a + b + new_extra_incr;
}

TEST(binpatch, individual_steps) {
  if (IF_GCC(true, false) || IF_DEBUG(true, false))
    return;
  Platform &platform = Platform::get();
  MessageSink messages;
  ASSERT_TRUE(platform.ensure_initialized(&messages));

  ASSERT_EQ(9, add(3, 5));
  PatchRequest patch(Code::upcast(add), Code::upcast(new_add));
  PatchSet patches(platform, Vector<PatchRequest>(&patch, 1));

  ASSERT_EQ(PatchSet::NOT_APPLIED, patches.status());
  ASSERT_TRUE(patches.prepare_apply(&messages));
  ASSERT_EQ(PatchSet::PREPARED, patches.status());

  ASSERT_TRUE(patches.open_for_patching(&messages));
  ASSERT_EQ(PatchSet::OPEN, patches.status());

  patches.install_redirects();
  ASSERT_EQ(PatchSet::APPLIED_OPEN, patches.status());

  ASSERT_TRUE(patches.close_after_patching(PatchSet::APPLIED, &messages));
  ASSERT_EQ(PatchSet::APPLIED, patches.status());

  ASSERT_EQ(10, add(3, 5));

  int (*add_imposter)(int, int) = patch.get_imposter(add);
  ASSERT_EQ(9, add_imposter(3, 5));

  ASSERT_TRUE(patches.open_for_patching(&messages));
  ASSERT_EQ(PatchSet::OPEN, patches.status());

  patches.revert_redirects();
  ASSERT_EQ(PatchSet::REVERTED_OPEN, patches.status());

  ASSERT_TRUE(patches.close_after_patching(PatchSet::NOT_APPLIED, &messages));
  ASSERT_EQ(PatchSet::NOT_APPLIED, patches.status());

  ASSERT_EQ(9, add(3, 5));
}

TEST(binpatch, address_range) {
  Platform &platform = Platform::get();
  MessageSink messages;
  ASSERT_TRUE(platform.ensure_initialized(&messages));

  PatchSet p0(platform, Vector<PatchRequest>());
  tclib::Blob r0 = p0.determine_address_range();
  ASSERT_PTREQ(NULL, r0.start());

  PatchRequest p1s[1] = {
    PatchRequest(CODE_UPCAST(7), NULL),
  };
  PatchSet p1(platform, Vector<PatchRequest>(p1s, 1));
  tclib::Blob r1 = p1.determine_address_range();
  ASSERT_PTREQ(reinterpret_cast<address_t>(7), r1.start());
  ASSERT_PTREQ(reinterpret_cast<address_t>(8), r1.end());

  PatchRequest p3s[3] = {
    PatchRequest(CODE_UPCAST(15), NULL),
    PatchRequest(CODE_UPCAST(10), NULL),
    PatchRequest(CODE_UPCAST(36), NULL)
  };
  PatchSet p3(platform, Vector<PatchRequest>(p3s, 3));
  tclib::Blob r3 = p3.determine_address_range();
  ASSERT_PTREQ(reinterpret_cast<address_t>(10), r3.start());
  ASSERT_PTREQ(reinterpret_cast<address_t>(37), r3.end());
}

TEST(binpatch, casts) {
  address_t add_addr = CODE_UPCAST(add_impl);
  int (*my_add)(int, int) = reinterpret_cast<int(*)(int, int)>(add_addr);
  ASSERT_TRUE(my_add == add_impl);
  ASSERT_EQ(8, my_add(3, 4));
  int (*my_add_2)(int, int) = Code::downcast(add_impl, add_addr);
  ASSERT_TRUE(my_add_2 == add_impl);
  ASSERT_EQ(8, my_add_2(3, 4));
  address_t add_addr_2 = Code::upcast(add_impl);
  ASSERT_PTREQ(add_addr, add_addr_2);
}

#define CHECK_LENGTH(EXP, ...) do {                                            \
  Disassembler &disass = Disassembler::x86_64();                               \
  byte_t bytes[16] = {__VA_ARGS__, 0x00};                                      \
  InstructionInfo info;                                                        \
  ASSERT_EQ(true, disass.resolve(Vector<byte_t>(bytes, 16), 0, &info));        \
  ASSERT_EQ(InstructionInfo::BENIGN, info.status());                           \
  ASSERT_EQ(EXP, info.length());                                               \
} while (false)

#define CHECK_STATUS(EXP, ...) do {                                            \
  Disassembler &disass = Disassembler::x86_64();                               \
  byte_t bytes[16] = {__VA_ARGS__, 0x00};                                      \
  InstructionInfo info;                                                        \
  ASSERT_EQ(false, disass.resolve(Vector<byte_t>(bytes, 16), 0, &info));       \
  ASSERT_EQ(InstructionInfo::EXP, info.status());                              \
  ASSERT_EQ(0, info.length());                                                 \
} while (false)

TEST(binpatch, x64_disass) {
  CHECK_LENGTH(1, 0x55); // push %rbp
  CHECK_LENGTH(3, 0x48, 0x89, 0xe5); // mov %rsp,%rbp
  CHECK_LENGTH(3, 0x89, 0x7d, 0xfc); // mov %edi,-0x4(%rbp)
  CHECK_LENGTH(3, 0x8B, 0x45, 0xfc); // mov %-0x8(%rbp),%eax
  CHECK_LENGTH(2, 0x01, 0xd0); // add %edx,%eax
  CHECK_LENGTH(1, 0x5d); // pop %rbp
  CHECK_LENGTH(7, 0x48, 0x89, 0x85, 0x48, 0xfe, 0xff, 0xff); // mov %rax,-0x1b8(%rbp)
  CHECK_LENGTH(3, 0x48, 0x89, 0xc7); // mov %rax,%rdi
  CHECK_LENGTH(4, 0x48, 0x89, 0x04, 0x24); // mov %rax,(%rsp)
  CHECK_STATUS(INVALID, 0x48, 0x48); // (two rex.w prefixes)
  CHECK_STATUS(INVALID, 0x60); // (pusha only exists in 32 bit mode)
}

static void check_jump_32(bool exp64, uint64_t a, uint64_t b) {
  bool expected = IF_32_BIT(true, exp64);
  ASSERT_EQ(expected, GenericX86::can_jump_relative_32(
      reinterpret_cast<address_t>(a), reinterpret_cast<address_t>(b)));
}

TEST(binpatch, can_jump_32) {
  uint64_t i30 = 1ULL << 30;
  check_jump_32(true, 0, i30);
  check_jump_32(true, i30, 0);
  uint64_t i31 = 1ULL << 31;
  check_jump_32(true, 0, i31 + 4);
  check_jump_32(true, 0, i31 + 5);
  check_jump_32(false, 0, i31 + 6);
  check_jump_32(false, 0, i31 + 7);
  check_jump_32(true, i31 - 7, 0);
  check_jump_32(true, i31 - 6, 0);
  check_jump_32(false, i31 - 5, 0);
  check_jump_32(false, i31 - 4, 0);
}

TEST(binpatch, saturate) {
  ASSERT_EQ(5, ProximityAllocator::minus_saturate_zero(8, 3));
  ASSERT_EQ(3, ProximityAllocator::minus_saturate_zero(8, 5));
  ASSERT_EQ(1, ProximityAllocator::minus_saturate_zero(8, 7));
  ASSERT_EQ(0, ProximityAllocator::minus_saturate_zero(8, 9));
  ASSERT_EQ(0, ProximityAllocator::minus_saturate_zero(8, 13));
  uint64_t u63 = 1ULL << 63;
  ASSERT_EQ(0, ProximityAllocator::minus_saturate_zero(8, u63));
  ASSERT_EQ(5, ProximityAllocator::minus_saturate_zero(u63 + 5, u63));
  uint64_t l64 = -1ULL;
  ASSERT_EQ(l64, ProximityAllocator::minus_saturate_zero(l64, 0));
  ASSERT_EQ(5, ProximityAllocator::minus_saturate_zero(l64, l64 - 5));
  ASSERT_EQ(0, ProximityAllocator::minus_saturate_zero(l64, l64));

  ASSERT_EQ(l64 - 2, ProximityAllocator::plus_saturate_max64(l64 - 5, 3));
  ASSERT_EQ(l64 - 1, ProximityAllocator::plus_saturate_max64(l64 - 5, 4));
  ASSERT_EQ(l64, ProximityAllocator::plus_saturate_max64(l64 - 5, 5));
  ASSERT_EQ(l64, ProximityAllocator::plus_saturate_max64(l64 - 5, 6));
  ASSERT_EQ(l64, ProximityAllocator::plus_saturate_max64(l64 - 5, 7));
  ASSERT_EQ(l64, ProximityAllocator::plus_saturate_max64(l64 - 5, u63));
  ASSERT_EQ(l64, ProximityAllocator::plus_saturate_max64(l64 - 5, l64));
}

static void check_anchor(uint64_t expected, uint64_t addr, uint64_t distance,
    uint32_t index) {
  ASSERT_EQ(expected, ProximityAllocator::get_anchor_from_address(addr,
      distance, index));
}

static void check_anchors(uint64_t bottom, uint64_t middle, uint64_t top,
    uint64_t addr, uint64_t distance) {
  check_anchor(bottom, addr, distance, 0);
  check_anchor(middle, addr, distance, ProximityAllocator::kAnchorCount / 2);
  check_anchor(top, addr, distance, ProximityAllocator::kAnchorCount);
}

TEST(binpatch, proxy_anchor) {
  check_anchors(1024 - 128, 1024, 1024 + 128, 1024, 256);
  check_anchors(1024 - 128, 1024, 1024 + 128, 1024 + 7, 256);
  check_anchors(1024 - 120, 1024 + 8, 1024 + 136, 1024 + 8, 256);
  check_anchors(1024 - 120, 1024 + 8, 1024 + 136, 1024 + 15, 256);
  check_anchors(1024 - 112, 1024 + 16, 1024 + 144, 1024 + 16, 256);
  check_anchors(1024 - 8, 1024 + 120, 1024 + 248, 1024 + 127, 256);

  uint64_t u32 = 1ULL << 32;
  uint64_t b32 = u32 >> 5;
  check_anchors(u32 - (16 * b32), u32, u32 + (16 * b32), u32, u32);
  check_anchors(u32 - (16 * b32), u32, u32 + (16 * b32), u32 + (b32 / 2), u32);
  check_anchors(u32 - (15 * b32), u32 + b32, u32 + (17 * b32), u32 + b32, u32);

  check_anchors(0, 0, 128, 0, 256);
  check_anchors(0, 64, 192, 64, 256);
  check_anchors(0, 128, 256, 128, 256);
  check_anchors(64, 192, 320, 192, 256);

  uint64_t u64 = -1ULL;
  uint64_t au64 = u64 - 7; // Max value aligned as an anchor.
  check_anchors(au64 - 128, au64, au64, u64, 256);
  check_anchors(au64 - 192, au64 - 64, au64, u64 - 64, 256);
  check_anchors(au64 - 256, au64 - 128, au64, u64 - 128, 256);
  check_anchors(au64 - 320, au64 - 192, au64 - 64, u64 - 192, 256);
}

class AllocRequest {
public:
  AllocRequest() : addr(0), size(0) { }
  AllocRequest(address_t _addr, size_t _size) : addr(_addr), size(_size) { }
  address_t addr;
  size_t size;
};

class TestVirtualAllocator : public VirtualAllocator {
public:
  enum behavior_t {
    ALWAYS_FAIL,
    ALWAYS_SUCCEED,
    SUCCEED_AFTER_7
  };
  TestVirtualAllocator(behavior_t behavior) : behavior_(behavior) { }
  typedef platform_hash_map<address_arith_t, AllocRequest> AllocMap;

  virtual tclib::Blob alloc_executable(address_t addr, size_t size,
      MessageSink *messages);
  virtual bool free_block(tclib::Blob block);
  AllocMap &allocs() { return allocs_; }
private:
  behavior_t behavior_;
  AllocMap allocs_;
};

tclib::Blob TestVirtualAllocator::alloc_executable(address_t addr, size_t size,
    MessageSink *messages) {
  bool should_succeed = (behavior_ == ALWAYS_SUCCEED)
      || ((behavior_ == SUCCEED_AFTER_7) && (allocs_.size() >= 7));
  address_arith_t key = reinterpret_cast<address_arith_t>(addr);
  ASSERT_EQ(0, allocs_.count(key));
  allocs_[key] = AllocRequest(addr, size);
  if (should_succeed) {
    address_t result = (addr == NULL) ? reinterpret_cast<address_t>(1 << 24) : addr;
    return tclib::Blob(result, size);
  } else {
    return tclib::Blob();
  }
}

bool TestVirtualAllocator::free_block(tclib::Blob block) {
  return true;
}

TEST(binpatch, failing_unrestricted) {
  TestVirtualAllocator direct(TestVirtualAllocator::ALWAYS_FAIL);
  size_t block_size = 1024 * 1024;
  ProximityAllocator alloc(&direct, 1024, block_size);

  // We haven't allocated in the underlying allocator yet.
  ASSERT_EQ(0, direct.allocs().size());

  // First allocation tries to grab virtual memory.
  tclib::Blob block = alloc.alloc_executable(0, 0, 8, NULL);
  ASSERT_TRUE(block.is_empty());
  ASSERT_EQ(1, direct.allocs().size());
  ASSERT_EQ(block_size, direct.allocs()[0].size);

  // If we failed the first time we don't try again, we just keep failing.
  tclib::Blob block2 = alloc.alloc_executable(0, 0, 8, NULL);
  ASSERT_TRUE(block2.is_empty());
  ASSERT_EQ(1, direct.allocs().size());
}

TEST(binpatch, failing_restricted) {
  TestVirtualAllocator direct(TestVirtualAllocator::ALWAYS_FAIL);
  size_t block_size = 1024 * 1024;
  ProximityAllocator alloc(&direct, 1024, block_size);

  // We haven't allocated in the underlying allocator yet.
  ASSERT_EQ(0, direct.allocs().size());

  address_arith_t u30 = 1 << 30;
  address_t a30 = reinterpret_cast<address_t>(u30);
  tclib::Blob block = alloc.alloc_executable(a30, 1 << 16, 8, NULL);
  ASSERT_TRUE(block.is_empty());
  ASSERT_EQ(ProximityAllocator::kAnchorCount, direct.allocs().size());
  ASSERT_EQ(block_size, direct.allocs()[u30].size);

  tclib::Blob block2 = alloc.alloc_executable(a30, 1 << 16, 8, NULL);
  ASSERT_TRUE(block2.is_empty());
  ASSERT_EQ(ProximityAllocator::kAnchorCount, direct.allocs().size());
}

TEST(binpatch, succeeding_unrestricted) {
  TestVirtualAllocator direct(TestVirtualAllocator::ALWAYS_SUCCEED);
  size_t alignment = 1024;
  size_t block_size = 1024 * 1024;
  ProximityAllocator alloc(&direct, alignment, block_size);

  // We haven't allocated in the underlying allocator yet.
  ASSERT_EQ(0, direct.allocs().size());

  // First allocation tries to grab virtual memory.
  tclib::Blob block = alloc.alloc_executable(0, 0, 8, NULL);
  ASSERT_EQ(alignment, block.size());
  ASSERT_EQ(1, direct.allocs().size());
  ASSERT_EQ(block_size, direct.allocs()[0].size);

  // If we failed the first time we don't try again, we just keep failing.
  tclib::Blob block2 = alloc.alloc_executable(0, 0, 8, NULL);
  ASSERT_EQ(alignment, block2.size());
  ASSERT_EQ(1, direct.allocs().size());
}

TEST(binpatch, succeeding_restricted) {
  TestVirtualAllocator direct(TestVirtualAllocator::SUCCEED_AFTER_7);
  size_t alignment = 1024;
  size_t block_size = 1024 * 1024;
  ProximityAllocator alloc(&direct, alignment, block_size);

  ASSERT_EQ(0, direct.allocs().size());

  address_arith_t u30 = 1 << 30;
  address_t a30 = reinterpret_cast<address_t>(u30);
  size_t dist = 1 << 24;
  tclib::Blob block = alloc.alloc_executable(a30, dist, 8, NULL);
  ASSERT_EQ(alignment, block.size());
  ASSERT_EQ(8, direct.allocs().size());
  ASSERT_TRUE(ProximityAllocator::is_within(u30, dist,
      reinterpret_cast<address_arith_t>(block.start())));
  ASSERT_TRUE(ProximityAllocator::is_within(u30, dist,
      reinterpret_cast<address_arith_t>(block.end())));

  tclib::Blob block2 = alloc.alloc_executable(a30, dist, 8, NULL);
  ASSERT_EQ(alignment, block2.size());
  ASSERT_EQ(8, direct.allocs().size());
  ASSERT_TRUE(ProximityAllocator::is_within(u30, dist,
      reinterpret_cast<address_arith_t>(block2.start())));
  ASSERT_TRUE(ProximityAllocator::is_within(u30, dist,
      reinterpret_cast<address_arith_t>(block2.end())));
}

// There's no reason these tests can't work on 32-bit, except some bugs with
// inline asm in gcc.
#if defined(IS_GCC) && defined(IS_64_BIT)
#  include "test_binpatch_posix.cc"
#endif
