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
  , stubs_(NULL) { }

PatchRequest::PatchRequest()
  : original_(NULL)
  , replacement_(NULL)
  , flags_(0)
  , stubs_(NULL) { }

void PatchRequest::preparing_apply(PatchStubs *stubs) {
  stubs->origin_ = this;
  stubs_ = stubs;
}

PatchSet::PatchSet(Platform &platform, Vector<PatchRequest> requests)
  : platform_(platform)
  , requests_(requests)
  , status_(NOT_APPLIED)
  , old_perms_(0) { }

bool PatchSet::prepare_apply() {
  if (requests().is_empty()) {
    // Trivially succeed if there are no patches to apply.
    status_ = PREPARED;
    return true;
  }
  // Determine the range within which all the original functions occur.
  Vector<byte_t> range = determine_patch_range();
  // Allocate a chunk of memory to hold the stubs.
  Vector<byte_t> memory = memory_manager().alloc_executable(range.start(),
      sizeof(PatchStubs) * requests().length());
  if (memory.is_empty()) {
    // We couldn't get any memory for the stubs; fail.
    LOG_ERROR("Failed to allocate memory for %i stubs near %p.",
        requests().length(), range.start());
    status_ = FAILED;
    return false;
  }
  // Calculate the distance between the two blocks of memory.
  size_t dist = range.distance(memory);
  // Check that it is possible to jump from within the range of the functions
  // to the stubs. This is a somewhat conservative estimate -- we're using the
  // worst case, the two locations farthest away from each other in the two
  // ranges, but the size of the ranges themselves should be negligible in
  // comparison and this seems like a safer check.
  if (!offsets_fit_in_32_bits(dist)) {
    LOG_ERROR("Memory couldn't be allocated near %p/%llu; distance to %p/%llu is %llu",
        range.start(), range.length(), memory.start(), memory.length(), dist);
    status_ = FAILED;
    return false;
  }
  // Cast the memory to a vector of patch stubs.
  stubs_ = memory.cast<PatchStubs>();
  for (size_t i = 0; i < requests().length(); i++)
    requests()[i].preparing_apply(&stubs_[i]);
  status_ = PREPARED;
  return true;
}

bool PatchSet::offsets_fit_in_32_bits(size_t dist) {
  return dist < 0x80000000;
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

Vector<byte_t> PatchSet::determine_patch_range() {
  Vector<byte_t> addr_range = determine_address_range();
  if (addr_range.is_empty()) {
    return addr_range;
  } else {
    // The amount we're going to write past the last address.
    size_t write_size = instruction_set().get_redirect_size_bytes() + 1;
    size_t new_length = addr_range.length() + write_size;
    return Vector<byte_t>(addr_range.start(), new_length);
  }
}

bool PatchSet::open_for_patching() {
  // Try opening the region for writing.
  Vector<byte_t> region = determine_patch_range();
  if (!memory_manager().open_for_writing(region, &old_perms_)) {
    LOG_ERROR("Failed to open code for writing.");
    status_= FAILED;
    return false;
  }
  // Validate that writing works.
  if (validate_open_for_patching()) {
    status_ = OPEN;
    return true;
  } else {
    status_ = FAILED;
    return false;
  }
}

bool PatchSet::validate_open_for_patching() {
  size_t redirect_size = instruction_set().get_redirect_size_bytes();
  for (size_t i = 0; i < requests().length(); i++) {
    // Try reading the each byte and then writing it back. This should not
    // have any effect but should fail if the memory is not writeable.
    //
    // TODO: check whether making the address volatile is enough for this to
    //   not be optimized away or otherwise tampered with by the compiler.
    volatile address_t addr = requests()[i].original();
    for (size_t i = 0; i < redirect_size; i++) {
      byte_t value = addr[i];
      addr[i] = value;
    }
  }
  return true;
}

bool PatchSet::close_after_patching(Status success_status) {
  Vector<byte_t> region = determine_patch_range();
  if (!memory_manager().close_for_writing(region, old_perms_)) {
    status_ = FAILED;
    return false;
  }
  status_ = success_status;
  return true;
}

void PatchSet::install_redirects() {
  for (size_t i = 0; i < requests().length(); i++)
    install_redirect(requests()[i]);
  status_ = APPLIED_OPEN;
}

void PatchSet::install_redirect(PatchRequest &request) {
  InstructionSet &inst = instruction_set();
  // Record the code being overwritten in the patch request.
  Vector<byte_t> overwritten = request.overwritten();
  memcpy(overwritten.start(), request.original(), inst.get_redirect_size_bytes());
  // Then install the redirect.
  inst.install_redirect(request);
}

void PatchSet::revert_redirects() {
  InstructionSet &inst = instruction_set();
  for (size_t i = 0; i < requests().length(); i++) {
    PatchRequest &request = requests()[i];
    Vector<byte_t> overwritten = request.overwritten();
    address_t original = request.original();
    memcpy(original, overwritten.start(), inst.get_redirect_size_bytes());
  }
  status_ = REVERTED_OPEN;
}


bool PatchSet::apply() {
  if (!prepare_apply())
    return false;
  if (!open_for_patching())
    return false;
  install_redirects();
  if (!close_after_patching(PatchSet::APPLIED))
    return false;
  return true;
}

Platform::Platform(InstructionSet &inst, MemoryManager &memman)
  : inst_(inst)
  , memman_(memman) { }

bool Platform::ensure_initialized() {
  return memory_manager().ensure_initialized();
}

// Returns the current platform.
Platform &Platform::get() {
  static Platform *instance = NULL;
  if (instance == NULL) {
    InstructionSet &inst = InstructionSet::get();
    MemoryManager &memman = MemoryManager::get();
    instance = new Platform(inst, memman);
  }
  return *instance;
}


#include "binpatch-x64.cc"

InstructionSet::~InstructionSet() {
}

InstructionSet &InstructionSet::get() {
  return X64::get();
}

#ifdef IS_MSVC
#include "binpatch-msvc.cc"
#else
#include "binpatch-posix.cc"
#endif
