//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// # Binary code patching
///
/// A binary patch in this context is one that causes the implementation of one
/// C function to be replaced by a different one without requiring changes to
/// the callers of the initial function. In other words, the function you think
/// you're calling is replaced with a different one under your feet.
///
/// The way this is done is by destructively overwriting the first instructions
/// of the function to be replaced with a jump to an alternative implementation.
/// This approach is somewhat inspired by code patching and somewhat by inline
/// caching techniques.
///
/// Code patching is tricky and error prone, and if you get it wrong it can do
/// real damage. For this reason this library is willing to sacrifice some
/// performance to have extra safeguards, and to refuse to work unless it is
/// convinced that it will be successful.
///
/// The library uses a few abstractions.
///
///    * A {{#PatchRequest}}(patch request) is a request to patch a function. It
///      also keeps track of the current state, whether it has been applied etc.
///    * The {{#PatchEngine}}(patch engine) encapsulates the mechanics of
///      actually making the code changes. Each platform has its own patch
///      engine.

#ifndef _BINPATCH
#define _BINPATCH

#include "c/stdhashmap.hh"
#include "c/stdvector.hh"
#include "utils/alloc.hh"
#include "utils/blob.hh"
#include "utils/fatbool.hh"
#include "utils/vector.hh"

BEGIN_C_INCLUDES
#include "utils/string.h"
END_C_INCLUDES

namespace conprx {

class ImposterCode;
class InstructionSet;
class MemoryManager;
class Platform;
class ProximityAllocator;

// The biggest possible redirect sequence. Since the preamble may be as large as
// 13 bytes the worst case it where the last instruction which starts at byte
// 12 is really long; this leaves room for it to be 20b which should be plenty.
#define kMaxPreambleSizeBytes 32

// This is just another name for a dword_t but can be used without including the
// full nightmare that is windows.h.
typedef uint32_t standalone_dword_t;

#define REPORT_MESSAGE(sink, ...) (sink)->report(__FILE__, __LINE__, __VA_ARGS__)

// Encapsulates state associated with redirecting a function. The main reason
// for separating this into a class is such that there's a place to store
// temporary data associated with redirecting and have it destroyed properly
// when the patch request is destroyed.
class Redirection : public tclib::DefaultDestructable {
public:
  // The different types of redirections supported. These exist mainly for
  // testing and debugging, there shouldn't be a need to dispatch on them.
  enum Type {
    rtAbs64,
    rtRel32,
    rtKangaroo
  };

  virtual ~Redirection() { }

  // Write the redirect to the given destination into the given code.
  virtual size_t write_redirect(address_t code, address_t dest) = 0;

  // Returns this redirection's type code. For testing.
  virtual Type type() = 0;
};

/// ## Patch request
///
/// An individual binary patch request is the "unit" of the patching process.
/// It contains an address of the function to replace and an address of its
/// intended replacement, as well as extra information tracking the state of
/// the patching process.
///
/// There's a number of different buffers and snippets in play here and it's
/// important to keep their different purposes straight. They are,
///
///   original: the address of the original function. Depending on whether the
///     patch has been applied calling the original will either have the
///     original behavior or the behavior of the replacement.
///
///   replacement: the address of the behavior we want the original function to
///     have after applying the patch.
///
///   preamble: the first part of the original function that we're going to
///     overwrite to give it the replacement behavior.
///
///   preamble copy: the contents of the preamble before it has been patched
///     which gets recorded in the patch request such that it can be used later,
///     for instance to restore the original back to its original behavior.
///
///   imposter: a function that when called implements the original behavior
///     even after the patch has been applied. So it's not the original function
///     which now has the replacement behavior but a different one that happens
///     to behave exactly like the original.
class PatchRequest {
public:
  enum Flags {
    // Don't allow 64-bit absolute jumps.
    pfBanAbs64 = 0x1,
    // Don't allow 32-bit relative jumps.
    pfBanRel32 = 0x2,
    // Don't allow kangaroo jumps (32-bit rel to 64-bit abs).
    pfBanKangaroo = 0x4
  };

  // Initializes a binary patch that replaces the given original function with
  // the given replacement.
  PatchRequest(address_t original, address_t replacement, const char *name = NULL);

  PatchRequest();

  ~PatchRequest();

  // Marks this request as having been prepared to be applied.
  fat_bool_t prepare_apply(Platform *platform, ProximityAllocator *alloc);

  // Destructively install this request's redirect.
  void install_redirect();

  // Attempts to apply this patch.
  fat_bool_t apply(MemoryManager &memman);

  address_t original() { return reinterpret_cast<address_t>(original_); }

  address_t replacement() { return reinterpret_cast<address_t>(replacement_); }

  size_t preamble_size() { return preamble_size_; }

  tclib::Blob imposter_code() { return imposter_code_; }

  Platform &platform() { return *platform_; }

  // Returns the trampoline code viewed as the given type.
  template <typename T>
  T get_imposter() { return reinterpret_cast<T>(get_or_create_imposter()); }

  // Returns the trampoline code viewed as the same type as the given argument.
  // Since function types are so long this can be a convenient shorthand.
  template <typename T>
  T get_imposter(T prototype) { return get_imposter<T>(); }

  tclib::Blob original_preamble() { return tclib::Blob(original_, preamble_size_); }

  tclib::Blob preamble_copy() { return tclib::Blob(preamble_copy_, preamble_size_); }

  // Returns the type of the redirection that was applied to perform this
  // patch. For testing.
  Redirection::Type redirection_type() { return redirection_->type(); }

  // Sets the flags that control how this patch is applied. The flags come from
  // the Flags enum.
  void set_flags(uint32_t flags) { flags_ = flags; }

  // Returns this patch's flag set.
  uint32_t flags() { return flags_; }

private:
  // Returns the address of the trampoline, creating it on first call.
  address_t get_or_create_imposter();

  // Writes the trampoline code.
  void write_imposter();

  // The address of the original function. Note that when the patch has been
  // applied the function at this address will effectively be the replacement,
  // to call the original use the generated trampoline which is a different
  // function but behaves as the original.
  address_t original_;

  // The address of the replacement function.
  address_t replacement_;

  // Optional display name, used for debugging.
  const char *name_;

  // A set of bit flags that control how this patch is applied.
  uint32_t flags_;

  // The completed imposter stub. Will be null until the imposter has been
  // written.
  address_t imposter_;

  // The custom code stubs associated with this patch.
  tclib::Blob imposter_code_;

  // A copy of the original method's preamble which we'll overwrite later on.
  byte_t preamble_copy_[kMaxPreambleSizeBytes];

  // The platform utilities.
  Platform *platform_;

  // The length of the original's preamble.
  size_t preamble_size_;

  // Object that keeps track of how to redirect this patch.
  tclib::def_ref_t<Redirection> redirection_;
};

// Size of the helper component of a patch stub.
#define kImposterCodeStubSizeBytes 32

/// ## Patch code
///
/// The imposter code struct is a container for the executable code used by a
/// patch to call the original code. Each patch request has an imposter code
/// struct associated with it.
///
/// To make the initial application of the patch set as cheap as possible we
/// initially only redirect and then generate the imposter on demand. The
/// request keeps track of the state of the code.
class ImposterCode {
public:
  tclib::Blob memory() { return tclib::Blob(stub_, kImposterCodeStubSizeBytes); }

private:
  byte_t stub_[kImposterCodeStubSizeBytes];
};

// All the platform dependent objects bundled together.
class Platform {
public:
  // Ensures that the platform object is fully initialized, if it hasn't been
  // already. Returns true on success.
  fat_bool_t ensure_initialized();

  MemoryManager &memory_manager() { return memman_; }

  InstructionSet &instruction_set() { return inst_; }

  // Returns the current platform. Remember to initialize it before using it.
  static Platform &get();

private:
  Platform(InstructionSet &inst, MemoryManager &memman);

  InstructionSet &inst_;
  MemoryManager &memman_;
};

// An allocator that allocates directly near an address, that is, it allocates
// either exactly at a given address or fails.
class VirtualAllocator {
public:
  virtual ~VirtualAllocator() { }
  virtual fat_bool_t alloc_executable(address_t addr, size_t size, tclib::Blob *blob_out) = 0;
  // Frees a block that was returned from alloc_executable. Returns true iff
  // freeing succeeds.
  virtual fat_bool_t free_block(tclib::Blob block) = 0;
};

// An allocator that allocates near an address and makes multiple attempts at
// different locations within a certain range and succeeds if any attempt
// succeeds, only failing if all attempts fail.
class ProximityAllocator {
public:
  // A block of memory that's already been allocated and that we can hand out
  // to callers.
  class Block {
  public:
    Block(uint64_t anchor)
      : is_restricted_(true)
      , is_active_(true)
      , next_(anchor)
      , limit_(anchor) { }

    Block()
      : is_restricted_(false)
      , is_active_(true)
      , next_(0)
      , limit_(0) { }

    // Returns true if allocating the given size within this block will yield
    // memory within the requested restrictions. Note that it's possible that
    // this block contains memory within the restrictions but if it's not the
    // next memory that would be returned this call will return false.
    bool can_provide(uint64_t addr, uint64_t distance, uint64_t size);

    // Attempt to make a region of the given size available within this block.
    // Returns true if a block of the given size can be allocated after this
    // call returns, false otherwise.
    bool ensure_capacity(uint64_t size, ProximityAllocator *owner);

    // Returns a block from this allocator.
    tclib::Blob alloc(uint64_t size);

    std::vector<tclib::Blob> *to_free() { return &to_free_; }

  private:
    std::vector<tclib::Blob> to_free_;
    bool is_restricted_;
    bool is_active_;
    uint64_t next_;
    uint64_t limit_;
  };

  typedef IF_32_BIT(uint32_t, uint64_t) anchor_key_t;

  typedef platform_hash_map<anchor_key_t, Block*> BlockMap;

  // Initialize this proximity allocator; allocate from the given direct
  // allocator, return memory with the given alignment (must be a power of 2),
  // allocate blocks in chunks of the given block_size (also power of 2).
  ProximityAllocator(VirtualAllocator *direct, uint64_t alignment, uint64_t block_size);

  ~ProximityAllocator();

  // Allocate a block of executable memory no further than the given distance
  // from the given address. Returns a block of memory if allocation succeeds,
  // otherwise an empty block. The distance must either be a power of 2 or 0
  // which means that there is no distance restriction, any memory is
  // acceptable. The distance must be a multiple of the allocator's block_size.
  tclib::Blob alloc_executable(address_t addr, uint64_t distance, size_t size);

  // If we already have a block that works for the given address returns it.
  Block *find_existing_block(uint64_t addr, uint64_t distance, uint64_t size);

  // Returns the unrestricted block, creating it if it doesn't already exist.
  Block *get_or_create_unrestricted();

  // Given an anchor, returns the block associated with that anchor, creating
  // it if it doesn't already exist.
  Block *get_or_create_anchor(uint64_t anchor);

  // Given an address and a distance returns the index'th anchor within that
  // region we'll attempt.
  static uint64_t get_anchor_from_address(uint64_t addr, uint64_t distance,
      uint32_t index);

  // Subtract b from a but return 0 in the case where the result would have
  // become negative had it been signed.
  static uint64_t minus_saturate_zero(uint64_t a, uint64_t b);

  // Add b to a but return max int 64 (2^64-1) in the case where the result
  // would have become larger had it had wider range.
  static uint64_t plus_saturate_max64(uint64_t a, uint64_t b);

  // Returns true iff the given address is within the given distance of the
  // given base address.
  static bool is_within(uint64_t base, uint64_t distance, uint64_t addr);

  // Deletes the given block along with any data held by it.
  bool delete_block(Block *block);

  static const size_t kLogAnchorCount = 5;
  static const size_t kAnchorCount = 1 << kLogAnchorCount;

private:
  // How many anchors within the requested distance from the address will we
  // try?
  static const uint32_t kAnchorOrder[ProximityAllocator::kAnchorCount];
  // The underlying allocator to ask for virtual memory.
  VirtualAllocator *direct_;
  // Memory allocated through this allocator will have this alignment.
  uint64_t alignment_;
  // The size of an individual block of memory to allocate. This must be larger
  // than any range requested.
  uint64_t block_size_;
  // A mapping from anchor addresses to the currently active block for that
  // anchor.
  Block *unrestricted_;
  BlockMap anchors_;
};

/// ## Patch set
///
/// A patch set holds a number of patch requests. We generally want to apply a
/// whole set of patches together since if we fail part way through the code
/// will be left in an inconsistent state.
///
/// The initial patch process happens in a couple of steps.
///
///   1. First we allocate and validate the executable stubs. At this point
///      no changes have been made to the original code so if this fails we
///      can bail out immediately. (`prepare_apply`).
///   2. Then we make the relevant memory regions holding the original code
///      writeable. If this fails part way through we have to revert the
///      permissions before aborting. We still haven't actually changed any
///      code. (`open_for_patching`).
///   3. Then we patch the code. It shouldn't be possible for this to fail since
///      the permissions are set up appropriately and the stubs have been
///      validated. (`install_redirects`)
///   4. Finally we revert the permissions on any memory we've written.
///      (`close_after_patching`).
///
/// At this point any calls to the patched functions will be redirected to the
/// replacements. The trampolines will be generated on demand.
class PatchSet {
public:
  // Indicates the current state of this patch set.
  enum Status {
    NOT_APPLIED,
    PREPARED,
    OPEN,
    APPLIED_OPEN,
    REVERTED_OPEN,
    APPLIED,
    FAILED
  };

  // Creates a patch set that applies the given list of patches.
  PatchSet(Platform &platform, Vector<PatchRequest> requests);

  // Prepares to apply the patches. Preparing makes no code changes, it only
  // checks whether patching is possible and allocates the data needed to do
  // the patching. Returns true iff preparing succeeded.
  fat_bool_t prepare_apply();

  // Attempts to set the permissions on the code we're going to patch. Returns
  // true iff this succeeds.
  fat_bool_t open_for_patching();

  // Attempts to return the permissions on the code that has been patched to
  // the state it was before.
  fat_bool_t close_after_patching(Status success_status);

  // Patches the redirect instructions into the original functions. It shouldn't
  // be possible for this to fail, any potential failure reasons should have
  // been checked in prepare_apply. It is important that prepare_apply catches
  // those problems because we want to avoid the case where we think the changes
  // can be applied but part way through we fail, potentially leaving the code
  // in a partially patched state which would be Bad. We definitely want to
  // fail gracefully before starting to overwrite code if at all possible.
  void install_redirects();

  // Restores the original functions to their initial state. The same
  // considerations about failing apply as with install_redirects.
  void revert_redirects();

  // Given a fresh patch set, runs through the sequence of operations that
  // causes the patches to be applied.
  fat_bool_t apply();

  // Runs through the complete sequence of operations that restores the
  // originals to their initial state.
  fat_bool_t revert();

  // Determines the lower and upper bound of the addresses to patch by scanning
  // over the requests and min/max'ing over the addresses. Note that this is
  // not the range we're going to write within but slightly narrower -- it
  // stops at the beginning of the last function to write and we'll be writing
  // a little bit past the beginning. Use determine_patch_range to get the
  // full range we'll be writing.
  tclib::Blob determine_address_range();

  // Does basically the same thing as determine_address_range but takes into
  // account that we have to write some amount past the last address. All the
  // memory we'll be writing will be within this range.
  tclib::Blob determine_patch_range();

  Status status() { return status_; }

  ProximityAllocator *alloc() { return &alloc_; }

private:
  // Attempts to write all the locations we'll be patching. Returns false or
  // crashes if writing fails.
  bool validate_open_for_patching();

  // The patch engine used to apply this patch set.
  Platform &platform_;
  MemoryManager &memory_manager() { return platform_.memory_manager(); }
  InstructionSet &instruction_set() { return platform_.instruction_set(); }

  // The patch requests to apply.
  Vector<PatchRequest> requests_;
  Vector<PatchRequest> &requests() { return requests_; }

  ProximityAllocator alloc_;

  // The current status.
  Status status_;

  // When the code is open for writing this field holds the previous
  // permissions set on the code.
  standalone_dword_t old_perms_;
};

// Casts a function to an address statically such that it can be used in
// compile time constants. If the result doesn't have to be a constant it's
// better to use Code::upcast.
#define CODE_UPCAST(EXPR) reinterpret_cast<address_t>(EXPR)

// Utilities for working with function pointers.
class Code {
public:
  // Casts the given address to the same type as the given prototype.
  template <typename T>
  static T *downcast(T *prototype, address_t addr) {
    return reinterpret_cast<T*>(addr);
  }

  // Casts the given function to an address.
  template <typename T>
  static address_t upcast(T *value) {
    return CODE_UPCAST(value);
  }
};

// The platform-specific object that knows how to allocate and manipulate memory.
class MemoryManager : public VirtualAllocator {
public:
  virtual ~MemoryManager();

  // If the manager needs any kind of initialization this call will ensure that
  // it has been initialized.
  virtual fat_bool_t ensure_initialized();

  // Attempts to open the given memory region such that it can be written. If
  // successful the previous permissions should be stored in the given out
  // parameter.
  virtual fat_bool_t open_for_writing(tclib::Blob region, standalone_dword_t *old_perms) = 0;

  // Attempts to close the given memory region such that it can no longer be
  // written. The old_perms parameter contains the value that was stored by
  // open_for_writing.
  virtual fat_bool_t close_for_writing(tclib::Blob region, standalone_dword_t old_perms) = 0;

  // Allocates a piece of executable memory of the given size near enough to
  // the given address that we can jump between them. If memory can't be
  // successfully allocated returns an empty vector.

  // Returns the memory manager appropriate for this platform.
  static MemoryManager &get();
};

// Information about the preamble returned when scanning over the original
// function.
class PreambleInfo {
public:
  // Initialize an empty info.
  PreambleInfo();

  // Store information about the preamble in this info.
  void populate(size_t size, byte_t last_instr);

  // Returns the preamble size in bytes.
  size_t size() { return size_; }

  // Returns the last instruction processed; this may or may not be included in
  // the preamble depending on which instruction it is. Useful for logging and
  // debugging, don't let behavior depend on this.
  byte_t last_instr() { return last_instr_; }

private:
  size_t size_;
  byte_t last_instr_;
};

// An instruction set encapsulates the flavor of machine code used on the
// current platform.
class InstructionSet {
public:
  virtual ~InstructionSet() { }

  // Given a blob covering the preamble available to be overwritten, the address
  // of the replacement, and a memory manager, returns a redirection object that
  // controls the redirection. This doesn't change the original function but may
  // allocate through the memory manager. If no redirection works under the
  // given circumstances NULL is returned.

  // Performs any preprocessing to be done before patching, including
  // calculating the size of the preamble of the original function that will be
  // overwritten by the redirect. Returns true if successful, false if there is
  // some reason the function can't be patched; if it returns false a message
  // will have been reported to the given message sink.
  virtual fat_bool_t prepare_patch(PatchRequest *request, ProximityAllocator *alloc,
      tclib::pass_def_ref_t<Redirection> *redir_out, PreambleInfo *info_out) = 0;

  // Fills the given memory with code that causes execution to halt.
  virtual void write_halt(tclib::Blob memory) = 0;

  // Writes trampoline code into the given code object that implements the same
  // behavior as the request's original function did before it was replaced.
  // Returns the number of bytes written.
  virtual size_t write_imposter(PatchRequest &request, tclib::Blob memory) = 0;

  // Notify the processor that there have been code changes.
  virtual void flush_instruction_cache(tclib::Blob memory) = 0;

  // Returns the instruction set to use on this platform.
  static InstructionSet &get();
};

} // namespace conprx

#endif // _BINPATCH
