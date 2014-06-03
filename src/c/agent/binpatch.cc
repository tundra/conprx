//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "binpatch.hh"
#include "utils/log.hh"

bool MemoryManager::ensure_initialized() {
  return true;
}

MemoryManager::~MemoryManager() {
}

PatchRequest::PatchRequest(address_t original, address_t replacement, uint32_t flags)
  : original_(original)
  , replacement_(replacement)
  , flags_(flags)
  , stubs_(NULL)
  , trampoline_(NULL)
  , old_perms_(0)
  , status_(NOT_APPLIED) { }

bool PatchRequest::is_tentatively_possible() {
  int64_t distance64 = get_relative_distance() - kJmpSizeBytes;
  int32_t distance32 = static_cast<int32_t>(distance64);
  if (static_cast<int64_t>(distance32) != distance64)
    // If the distance doesn't fit in a 32-bit signed int we can't encode a
    // jmp instruction so the patch definitely won't work.
    return false;
  return true;
}

int64_t PatchRequest::get_relative_distance() {
  address_t original_addr = static_cast<address_t>(original_);
  address_t replacement_addr = static_cast<address_t>(replacement_);
  return replacement_addr - original_addr;
}

void PatchRequest::preparing_apply(PatchStubs *stubs) {
  stubs->origin_ = this;
  stubs_ = stubs;
  status_ = PREPARED;
}

bool PatchRequest::apply(MemoryManager &memman) {
  address_t original_addr = static_cast<address_t>(original_);
  // Ensure that the function can be written.
  if (!memman.try_open_page_for_writing(original_addr, &old_perms_)) {
    status_ = FAILED;
    return false;
  }
  // Try building the trampoline before patching since we need the contents of
  // the original function.
  if ((flags_ & MAKE_TRAMPOLINE) != 0) {
    trampoline_ = build_trampoline(memman);
    if (trampoline_ == NULL)
      // TODO: This should really be moved to the tentative check since it's
      //   possible and it would be safer to fail up front rather than during
      //   application.
      return false;
  }
  // Save the code we're overwriting so it can be restored.
  memcpy(overwritten_, original_addr, kPatchSizeBytes);
  // Write the patch.
  int32_t dist = get_relative_distance() - kJmpSizeBytes;
  original_addr[0] = kJmpOp;
  reinterpret_cast<int32_t*>(original_addr + 1)[0] = dist;
  // Make sure to close the memory again so we don't leave it w+x.
  if (!memman.try_close_page_for_writing(original_addr, old_perms_)) {
    status_ = FAILED;
    return false;
  }
  status_ = APPLIED;
  return true;
}

bool PatchRequest::revert(MemoryManager &memman) {
  address_t original_addr = static_cast<address_t>(original_);
  // Ensure that the function can be written.
  if (!memman.try_open_page_for_writing(original_addr, &old_perms_))
    return false;
  // Restore the instructions that were overwritten.
  memcpy(original_addr, overwritten_, kPatchSizeBytes);
  // Make sure we close the memory again so we don't leave it w+x.
  if (!memman.try_close_page_for_writing(original_addr, old_perms_))
    return false;
  status_ = NOT_APPLIED;
  return true;
}

byte_t *PatchRequest::build_trampoline(MemoryManager &memman) {
  size_t preamble_size = get_preamble_size(original_);
  if (preamble_size == 0) {
    uint32_t *blocks = reinterpret_cast<uint32_t*>(original_);
    LOG_ERROR("Couldn't determine preamble size of %p [code: %x %x %x %x]",
        original_, blocks[0], blocks[1], blocks[2], blocks[3]);
    // It wasn't possible to determine how long the preamble is so bail.
    return NULL;
  }
  // size_t trampoline_size = preamble_size + kResumeSizeBytes;
  byte_t *result = NULL; // engine.alloc_executable(original_, trampoline_size);
  if (result == NULL)
    return result;
  memcpy(result, original_, preamble_size);
  result[preamble_size] = kJmpOp;
  int32_t dist = original_ - result - kResumeSizeBytes;
  (reinterpret_cast<int32_t*>(result + preamble_size + 1))[0] = dist;
  return result;
}

size_t PatchRequest::get_preamble_size(address_t code) {
  if (code[0] == 0x55) {
    // push %rbp
    if (code[1] == 0x48 && code[2] == 0x89 && code[3] == 0xe5) {
      // mov %rsp,%rbp
      if (code[4] == 0x89 && code[5] == 0x7d && code[6] == 0xFC) {
        // mov %edi,-0x4(%rbp)
        // Standard x64 C function preamble.
        return 7;
      }
    }
  }
  // None of the patterns matched so we daren't patch.
  return 0;
}

PatchSet::PatchSet(MemoryManager &memman, Vector<PatchRequest> requests)
  : memman_(memman)
  , requests_(requests)
  , status_(NOT_APPLIED) { }

bool PatchSet::prepare_apply() {
  if (requests().is_empty())
    // Trivially succeed if there are no patches to apply.
    return true;
  // Determine the range within which all the original functions occur.
  Vector<byte_t> range = determine_address_range();
  // Allocate a chunk of memory to hold the stubs.
  Vector<byte_t> memory = memman().alloc_executable(range.start(),
      sizeof(PatchStubs) * requests().length());
  if (memory.is_empty()) {
    // We couldn't get any memory for the stubs; fail.
    LOG_ERROR("Failed to allocate memory for %i requests near %p.",
        range.start(), requests().length());
    return false;
  }
  // Calculate the distance between the two blocks of memory.
  size_t dist = range.distance(memory);
  // Check that it is possible to jump from within the range of the functions
  // to the stubs. This is a somewhat conservative estimate -- we're using the
  // worst case, the two locations farthest away from each other in the two
  // ranges, but the size of the ranges themselves should be negligible in
  // comparison and this seems like a safer check.
  if (!offsets_fits_in_32_bits(dist)) {
    LOG_ERROR("Memory couldn't be allocated near %p/%llu; distance to %p/%llu is %llu",
        range.start(), range.length(), memory.start(), memory.length(), dist);
    return false;
  }
  // Cast the memory to a vector of patch stubs.
  stubs_ = memory.cast<PatchStubs>();
  for (size_t i = 0; i < requests().length(); i++)
    requests()[i].preparing_apply(&stubs_[i]);
  return true;
}

bool PatchSet::offsets_fits_in_32_bits(size_t dist) {
  return (dist & ~static_cast<size_t>(0x7FFFFFFF)) == 0;
}

Vector<byte_t> PatchSet::determine_address_range() {
  if (requests().length() == 0) {
    // There are no patches so the range is empty.
    return Vector<byte_t>();
  } else {
    // Scan through the patches to determine the range. Note that the result
    // will have its endpoint one past the highest address, hence the +1 at the
    // end.
    address_t lowest;
    address_t highest;
    lowest = highest = requests()[0].original();
    for (size_t i = 1; i < requests().length(); i++) {
      address_t addr = requests()[i].original();
      if (addr < lowest)
        lowest = addr;
      if (addr > highest)
        highest = addr;
    }
    return Vector<byte_t>(lowest, highest + 1);
  }
}

#ifdef IS_MSVC
#include "binpatch-msvc.cc"
#else
#include "binpatch-posix.cc"
#endif
