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
  // Ensure that the function can be written.
  if (!engine.try_open_page_for_writing(original_addr, &old_perms_)) {
    status_ = FAILED;
    return false;
  }
  // Try building the trampoline before patching since we need the contents of
  // the original function.
  trampoline_ = build_trampoline(engine);
  if (trampoline_ == NULL)
    // TODO: This should really be moved to the tentative check since it's
    //   possible and it would be safer to fail up front rather than during
    //   application.
    return false;
  // Save the code we're overwriting so it can be restored.
  memcpy(overwritten_, original_addr, kPatchSizeBytes);
  // Write the patch.
  int32_t dist = get_relative_distance() - kJmpSizeBytes;
  original_addr[0] = kJmpOp;
  reinterpret_cast<int32_t*>(original_addr + 1)[0] = dist;
  // Make sure to close the memory again so we don't leave it w+x.
  if (!engine.try_close_page_for_writing(original_addr, old_perms_)) {
    status_ = FAILED;
    return false;
  }
  status_ = APPLIED;
  return true;
}

bool BinaryPatch::revert(PatchEngine &engine) {
  address_t original_addr = static_cast<address_t>(original_);
  // Ensure that the function can be written.
  if (!engine.try_open_page_for_writing(original_addr, &old_perms_))
    return false;
  // Restore the instructions that were overwritten.
  memcpy(original_addr, overwritten_, kPatchSizeBytes);
  // Make sure we close the memory again so we don't leave it w+x.
  if (!engine.try_close_page_for_writing(original_addr, old_perms_))
    return false;
  status_ = NOT_APPLIED;
  return true;
}

byte_t *BinaryPatch::build_trampoline(PatchEngine &engine) {
  address_t original_addr = static_cast<address_t>(original_);
  size_t preamble_size = get_preamble_size(original_addr);
  if (preamble_size == 0)
    return 0;
  size_t trampoline_size = preamble_size + kResumeSizeBytes;
  byte_t *result = engine.alloc_executable(original_addr, trampoline_size);
  if (result == NULL)
    return result;
  memcpy(result, original_, preamble_size);
  result[preamble_size] = kJmpOp;
  int32_t dist = original_addr - result - kResumeSizeBytes;
  (reinterpret_cast<int32_t*>(result + preamble_size + 1))[0] = dist;
  return result;
}

size_t BinaryPatch::get_preamble_size(address_t code) {
  if (code[0] == 0x55) {
    // push %rbp
    if (code[1] == 0x48 && code[2] == 0x89 && code[3] == 0xe5) {
      // mov %rsp,%rbp
      if (code[4] == 0x89 && code[5] == 0x7d && code[6] == 0xFC) {
        // mov %edi,-0x4(%rbp)
        // Standard C calling conventions.
        return 7;
      }
    }
  }
  // None of the patterns matched so we daren't patch.
  return 0;
}

#ifdef IS_MSVC
#include "binpatch-msvc.cc"
#else
#include "binpatch-posix.cc"
#endif
