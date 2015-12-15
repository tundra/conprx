//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "binpatch.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace conprx;
using namespace tclib;

bool MemoryManager::ensure_initialized(MessageSink *messages) {
  return true;
}

MemoryManager::~MemoryManager() {
}

MessageSink::~MessageSink() {
  for (uint32_t i = 0; i < messages_.size(); i++)
    free(messages_[i]);
  messages_.clear();
}

bool MessageSink::report(const char *file, int line, const char *fmt, ...) {
  // Record the message in the sink.
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_buffer_printf(&buf, "%s: %i: ", file, line);
  va_list argp;
  va_start(argp, fmt);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  utf8_t result = string_buffer_flush(&buf);
  char *message = strdup(result.chars);
  messages_.push_back(message);
  string_buffer_dispose(&buf);
  // Also report the message through normal logging.
  WARN("Reported %s", message);
  return false;
}

PatchRequest::PatchRequest(address_t original, address_t replacement, const char *name)
  : original_(original)
  , replacement_(replacement)
  , name_(name)
  , trampoline_(NULL)
  , trampoline_code_(NULL)
  , platform_(NULL)
  , preamble_size_(0) { }

PatchRequest::PatchRequest()
  : original_(NULL)
  , replacement_(NULL)
  , name_(NULL)
  , trampoline_(NULL)
  , trampoline_code_(NULL)
  , platform_(NULL)
  , preamble_size_(0) { }

bool PatchRequest::prepare_apply(Platform *platform, PatchCode *code,
    MessageSink *messages) {
  platform_ = platform;
  trampoline_code_ = code;
  // Determine the length of the preamble. This also determines whether the
  // preamble can safely be overwritten, otherwise we'll bail out.
  DEBUG("Preparing %s", name_);
  if (!platform->instruction_set().prepare_patch(original_, replacement_,
      trampoline_, platform->instruction_set().redirect_size_bytes(),
      &preamble_size_, messages))
    return false;
  memcpy(preamble_, original_, preamble_size_);
  return true;
}

address_t PatchRequest::get_or_create_trampoline() {
  if (trampoline_ == NULL)
    write_trampoline();
  return trampoline_;
}

void PatchRequest::write_trampoline() {
  InstructionSet &inst = platform().instruction_set();
  address_t addr = code().trampoline_;
  inst.write_trampoline(*this, code());
  inst.flush_instruction_cache(Blob(addr, kTrampolinePatchStubSizeBytes));
  trampoline_ = addr;
}

PatchSet::PatchSet(Platform &platform, Vector<PatchRequest> requests)
  : platform_(platform)
  , requests_(requests)
  , status_(NOT_APPLIED)
  , old_perms_(0) { }

bool PatchSet::prepare_apply(MessageSink *messages) {
  DEBUG("Preparing to apply patch set");
  if (requests().is_empty()) {
    // Trivially succeed if there are no patches to apply.
    status_ = PREPARED;
    return true;
  }
  // Determine the range within which all the original functions occur.
  Vector<byte_t> range = determine_patch_range();
  DEBUG("Patch range: %p .. %p", range.start(), range.end());
  // Allocate a chunk of memory to hold the stubs.
  Vector<byte_t> memory = memory_manager().alloc_executable(range.start(),
      sizeof(PatchCode) * requests().length(), messages);
  DEBUG("Memory: %p .. %p", memory.start(), memory.end());
  if (memory.is_empty()) {
    // We couldn't get any memory for the stubs; fail.
    LOG_ERROR("Failed to allocate memory for %i stubs near %p.",
        requests().length(), range.start());
    status_ = FAILED;
    return false;
  }
  codes_ = memory.cast<PatchCode>();
  for (size_t i = 0; i < requests().length(); i++) {
    if (!requests()[i].prepare_apply(&platform_, &codes_[i], messages)) {
      status_ = FAILED;
      return false;
    }
  }
  status_ = PREPARED;
  return true;
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
    size_t write_size = instruction_set().redirect_size_bytes() + 1;
    size_t new_length = addr_range.length() + write_size;
    return Vector<byte_t>(addr_range.start(), new_length);
  }
}

bool PatchSet::open_for_patching(MessageSink *messages) {
  DEBUG("Opening original code for writing");
  // Try opening the region for writing.
  Vector<byte_t> region = determine_patch_range();
  if (!memory_manager().open_for_writing(region, &old_perms_, messages)) {
    status_= FAILED;
    return false;
  }
  // Validate that writing works.
  DEBUG("Validating that code is writable");
  if (validate_open_for_patching(messages)) {
    status_ = OPEN;
    return true;
  } else {
    status_ = FAILED;
    return false;
  }
}

bool PatchSet::validate_open_for_patching(MessageSink *messages) {
  size_t redirect_size = instruction_set().redirect_size_bytes();
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

bool PatchSet::close_after_patching(Status success_status, MessageSink *messages) {
  DEBUG("Closing original code for writing");
  Vector<byte_t> region = determine_patch_range();
  if (!memory_manager().close_for_writing(region, old_perms_, messages)) {
    status_ = FAILED;
    return false;
  }
  instruction_set().flush_instruction_cache(region.memory());
  DEBUG("Successfully closed original code");
  status_ = success_status;
  return true;
}

void PatchSet::install_redirects() {
  DEBUG("Installing redirects");
  InstructionSet &inst = instruction_set();
  for (size_t i = 0; i < requests().length(); i++)
    inst.install_redirect(requests()[i]);
  DEBUG("Successfully installed redirects");
  status_ = APPLIED_OPEN;
}

void PatchSet::revert_redirects() {
  DEBUG("Reverting redirects");
  for (size_t i = 0; i < requests().length(); i++) {
    PatchRequest &request = requests()[i];
    Vector<byte_t> preamble = request.preamble();
    address_t original = request.original();
    memcpy(original, preamble.start(), preamble.length());
  }
  DEBUG("Successfully reverted redirects");
  status_ = REVERTED_OPEN;
}


bool PatchSet::apply(MessageSink *messages) {
  if (!prepare_apply(messages))
    return false;
  if (!open_for_patching(messages))
    return false;
  install_redirects();
  if (!close_after_patching(PatchSet::APPLIED, messages))
    return false;
  return true;
}

bool PatchSet::revert(MessageSink *messages) {
  if (!open_for_patching(messages))
    return false;
  revert_redirects();
  if (!close_after_patching(PatchSet::NOT_APPLIED, messages))
    return false;
  return true;
}

Platform::Platform(InstructionSet &inst, MemoryManager &memman)
  : inst_(inst)
  , memman_(memman) { }

bool Platform::ensure_initialized(MessageSink *messages) {
  return memory_manager().ensure_initialized(messages);
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


#ifdef IS_64_BIT
#  include "binpatch-x86-64.cc"
#else
#  include "binpatch-x86-32.cc"
#endif

#ifdef IS_MSVC
#include "binpatch-msvc.cc"
#else
#include "binpatch-posix.cc"
#endif
