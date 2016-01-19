//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "binpatch.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/misc-inl.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace conprx;
using namespace tclib;

bool MemoryManager::ensure_initialized(MessageSink *messages) {
  return true;
}

MemoryManager::~MemoryManager() {
}


bool MessageSink::report(const char *file, int line, const char *fmt, ...) {
  // Record the message in the sink.
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_buffer_printf(&buf, "%s:%i: ", file, line);
  va_list argp;
  va_start(argp, fmt);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  utf8_t result = string_buffer_flush(&buf);
  handle_message(result);
  string_buffer_dispose(&buf);
  return false;
}

void MessageSink::handle_message(utf8_t message) {
  WARN("Message: %s", message.chars);
}

PatchRequest::PatchRequest(address_t original, address_t replacement, const char *name)
  : original_(original)
  , replacement_(replacement)
  , name_(name)
  , imposter_(NULL)
  , imposter_code_(NULL)
  , platform_(NULL)
  , preamble_size_(0)
  , redirection_(NULL) { }

PatchRequest::PatchRequest()
  : original_(NULL)
  , replacement_(NULL)
  , name_(NULL)
  , imposter_(NULL)
  , imposter_code_(NULL)
  , platform_(NULL)
  , preamble_size_(0)
  , redirection_(NULL) { }

PatchRequest::~PatchRequest() {
  delete redirection_;
}

bool PatchRequest::prepare_apply(Platform *platform, ImposterCode *code,
    MessageSink *messages) {
  platform_ = platform;
  imposter_code_ = code;
  // Determine the length of the preamble. This also determines whether the
  // preamble can safely be overwritten, otherwise we'll bail out.
  DEBUG("Preparing %s", name_);
  PreambleInfo pinfo;

  Redirection *redir = platform->instruction_set().prepare_patch(original_,
      replacement_, &pinfo, messages);
  if (redir == NULL)
    return false;
  redirection_ = redir;
  size_t size = pinfo.size();
  CHECK_REL("preamble too big", size, <=, kMaxPreambleSizeBytes);
  memcpy(preamble_copy_, original_, pinfo.size());
  preamble_size_ = size;
  return true;
}

address_t PatchRequest::get_or_create_imposter() {
  if (imposter_ == NULL)
    write_imposter();
  return imposter_;
}

void PatchRequest::write_imposter() {
  InstructionSet &inst = platform().instruction_set();
  tclib::Blob imposter_memory = imposter_code()->memory();
  // Clear the memory to halt such that if the imposter doesn't fill the memory
  // completely there'll be sane code there.
  inst.write_halt(imposter_memory);
  inst.write_imposter(*this, imposter_memory);
  inst.flush_instruction_cache(imposter_memory);
  imposter_ = static_cast<address_t>(imposter_memory.start());
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
  Vector<byte_t> trampoline_memory = memory_manager().alloc_executable(NULL,
      sizeof(ImposterCode) * requests().length(), messages);
  if (trampoline_memory.is_empty())
    return REPORT_MESSAGE(messages, "Failed to allocate trampolines");
  codes_ = trampoline_memory.cast<ImposterCode>();
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
    size_t new_length = addr_range.length() + kMaxPreambleSizeBytes;
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
  for (size_t i = 0; i < requests().length(); i++) {
    // Try reading the each byte and then writing it back. This should not
    // have any effect but should fail if the memory is not writeable.
    //
    // TODO: check whether making the address volatile is enough for this to
    //   not be optimized away or otherwise tampered with by the compiler.
    volatile address_t addr = requests()[i].original();
    for (size_t i = 0; i < kMaxPreambleSizeBytes; i++) {
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
  for (size_t ri = 0; ri < requests().length(); ri++) {
    PatchRequest &request = requests()[ri];
    request.install_redirect();
  }
  DEBUG("Successfully installed redirects");
  status_ = APPLIED_OPEN;
}

void PatchRequest::install_redirect() {
  CHECK_TRUE("no redirection", redirection_ != NULL);
  // First clear the whole preamble to halt such that if there is any space left
  // over after writing the redirect it will be valid instructions but shouldn't
  // be useful to anyone.
  tclib::Blob orig_preamble(original_, preamble_size_);
  platform().instruction_set().write_halt(orig_preamble);
  redirection_->write_redirect(original_, replacement_);
}

void PatchSet::revert_redirects() {
  DEBUG("Reverting redirects");
  for (size_t i = 0; i < requests().length(); i++) {
    PatchRequest &request = requests()[i];
    Vector<byte_t> preamble = request.preamble_copy();
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

PreambleInfo::PreambleInfo()
  : size_(0)
  , last_instr_(0) { }

void PreambleInfo::populate(size_t size, byte_t last_instr) {
  size_ = size;
  last_instr_ = last_instr;
}

ProximityAllocator::ProximityAllocator(VirtualAllocator *direct, uint64_t alignment,
    uint64_t block_size)
  : direct_(direct)
  , alignment_(alignment)
  , block_size_(block_size) {
  CHECK_TRUE("unaligned alignment", is_power_of_2(alignment));
  CHECK_TRUE("unaligned block size", is_power_of_2(block_size));
}

ProximityAllocator::Block *ProximityAllocator::find_existing_block(uint64_t addr,
    uint64_t distance, uint64_t size) {
  BlockMap::iterator iter = anchors_.begin();
  for (; iter != anchors_.end(); iter++) {
    Block *candidate = &iter->second;
    if (candidate->can_provide(addr, distance, size))
      return candidate;
  }
  return NULL;
}

bool ProximityAllocator::Block::can_provide(uint64_t addr, uint64_t distance,
    uint64_t size) {
  return is_within(addr, distance, next_) && is_within(addr, distance, next_ + size);
}

bool ProximityAllocator::Block::is_within(uint64_t base, uint64_t distance,
    uint64_t addr) {
  uint64_t half_distance = distance >> 1;
  uint64_t min = minus_saturate_zero(base, half_distance);
  uint64_t max = plus_saturate_max64(base, half_distance);
  return (min <= addr) && (addr < max);
}

Vector<byte_t> ProximityAllocator::alloc_executable(address_t raw_addr,
    uint64_t distance, size_t size, MessageSink *messages) {
  uint64_t addr = reinterpret_cast<uint64_t>(raw_addr);
  Block *block = find_existing_block(addr, distance, size);
  if (block != NULL)
    return Vector<byte_t>();
//  address_t bottom_anchor = bottom_anchor_from_address(addr, distance);
  return Vector<byte_t>();
}

uint64_t ProximityAllocator::bottom_anchor_from_address(uint64_t addr,
    uint64_t distance) {
  if (distance == 0)
    // If there is no distance restriction we use the trivial bottom anchor.
    return 0;
  CHECK_TRUE("unaligned distance", is_power_of_2(distance));
  uint64_t addr_value = reinterpret_cast<uint64_t>(addr);
  // There are kAnchorCount anchors within the distance and the alignment and
  // this is the alignment they snap to.
  uint64_t anchor_alignment = distance >> kLogAnchorCount;
  // Align the address as an anchor; this will yield the anchor in the middle of
  // the range.
  uint64_t middle_anchor_value = addr_value & ~(anchor_alignment - 1);
  // Subtract half the distance; this will yield the bottom anchor.
  uint64_t half_distance = (distance >> 1);
  uint64_t bottom_anchor_value = minus_saturate_zero(middle_anchor_value,
      half_distance);
  return bottom_anchor_value;
}

uint64_t ProximityAllocator::minus_saturate_zero(uint64_t a, uint64_t b) {
  return (a <= b) ? 0 : (a - b);
}

uint64_t ProximityAllocator::plus_saturate_max64(uint64_t a, uint64_t b) {
  return (a <= (-1ULL - b)) ? (a + b) : -1ULL;
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
