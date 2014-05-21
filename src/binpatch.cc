//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "binpatch.hh"

bool PatchEngine::ensure_initialized() {
  return true;
}

PatchEngine::~PatchEngine() {
}

BinaryPatch::BinaryPatch(function_t original, function_t replacement)
  : original_(original)
  , replacement_(replacement)
  , trampoline_(NULL)
  , old_perms_(0)
  , status_(NOT_APPLIED) { }

bool BinaryPatch::is_tentatively_possible() {
  int64_t distance64 = get_relative_distance() - kJmpSizeBytes;
  int32_t distance32 = static_cast<int32_t>(distance64);
  if (static_cast<int64_t>(distance32) != distance64)
    // If the distance doesn't fit in a 32-bit signed int we can't encode a
    // jmp instruction so the patch definitely won't work.
    return false;
  return true;
}

int64_t BinaryPatch::get_relative_distance() {
  address_t original_addr = static_cast<address_t>(original_);
  address_t replacement_addr = static_cast<address_t>(replacement_);
  return replacement_addr - original_addr;
}

bool BinaryPatch::apply(PatchEngine &engine) {
  address_t original_addr = static_cast<address_t>(original_);
  if (!engine.try_open_page_for_writing(original_addr, &old_perms_)) {
    status_ = FAILED;
    return false;
  }
  memcpy(overwritten_, original_addr, kPatchSizeBytes);
  int32_t dist = get_relative_distance() - kJmpSizeBytes;
  original_addr[0] = kJmpOp;
  reinterpret_cast<int32_t*>(original_addr + 1)[0] = dist;
  if (!engine.try_close_page_for_writing(original_addr, old_perms_)) {
    status_ = FAILED;
    return false;
  }
  status_ = APPLIED;
  return true;
}

bool BinaryPatch::revert(PatchEngine &engine) {
  address_t original_addr = static_cast<address_t>(original_);
  if (!engine.try_open_page_for_writing(original_addr, &old_perms_))
    return false;
  memcpy(original_addr, overwritten_, kPatchSizeBytes);
  if (!engine.try_close_page_for_writing(original_addr, old_perms_))
    return false;
  status_ = NOT_APPLIED;
  return true;
}

#ifdef IS_MSVC
#include "binpatch-msvc.cc"
#else
#include "binpatch-posix.cc"
#endif
