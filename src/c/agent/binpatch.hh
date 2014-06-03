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

#include "utils/types.hh"
#include "utils/vector.hh"

class MemoryManager;
struct PatchStubs;

// The size in bytes of a jmp instruction.
#define kJmpSizeBytes 5

// The opcode of a jmp.
#define kJmpOp 0xE9

// The size in bytes of the patch that will be written into the subject function
// to redirect it.
#define kPatchSizeBytes kJmpSizeBytes

// The size in bytes of the instruction that resumes the original method after
// executing the preamble in the trampoline.
#define kResumeSizeBytes kJmpSizeBytes

// Shorthand for bytes.
typedef unsigned char byte_t;

// Byte-size memory address (so addition increments by one byte at a time).
typedef byte_t *address_t;

// Integer datatype large enough to allow address arithmetic.
typedef size_t address_arith_t;

/// ## Patch request
///
/// An individual binary patch request is the "unit" of the patching process.
/// It contains an address of the function to replace and an address of its
/// intended replacement, as well as extra information tracking the state of
/// the patching process.
class PatchRequest {
public:
  // Indicates the current state of this patch.
  enum Status {
    // This patch hasn't been applied yet, or it has been applied and then
    // successfully reverted.
    NOT_APPLIED,
    PREPARED,
    APPLIED,
    FAILED
  };

  // Flags that control how the patch is applied.
  enum Flag {
    MAKE_TRAMPOLINE = 0x1
  };

  // Initializes a binary patch that replaces the given original function with
  // the given replacement.
  PatchRequest(address_t original, address_t replacement, uint32_t flags = MAKE_TRAMPOLINE);

  // Marks this request as having been prepared to be applied.
  void preparing_apply(PatchStubs *stubs);

  // Checks, without making any changes, whether the given request may be
  // possible. This is to catch cases where we can tell early on that a patch
  // couldn't possibly succeed, without necessarily guaranteeing that it will
  // succeed if you try. The function that does the actual patching can assume
  // that this has been called and returned true.
  bool is_tentatively_possible();

  // Returns the current status.
  Status status() { return status_; }

  // Attempts to apply this patch.
  bool apply(MemoryManager &memman);

  address_t original() { return reinterpret_cast<address_t>(original_); }

  // Attempts to revert the patched code to its original state.
  bool revert(MemoryManager &memman);

  // When this patch is applied, returns the trampoline that behaves the same
  // way as the original implementation would have if it hadn't been patched.
  // The argument is just a convenient way to ensure that the result is cast
  // to the appropriate type if you pass in the original function.
  template <typename T>
  T get_trampoline(T prototype) {
    return reinterpret_cast<T>(trampoline_);
  }

private:
  // Returns the distance a relative jump has to travel to get from the original
  // to the replacement.
  int64_t get_relative_distance();

  // Builds the trampoline code that will implement the original function. If
  // for some reason a trampoline can't be built NULL is returned.
  byte_t *build_trampoline(MemoryManager &memman);

  // Returns the size of the trampoline preamble to use for the trampoline that
  // behaves like the function starting at the given address. If it is not
  // possible to trampoline this returns 0.
  static size_t get_preamble_size(address_t addr);

  // The address of the original function. Note that when the patch has been
  // applied the function at this address will effectively be the replacement,
  // to call the original use the generated trampoline which is a different
  // function but behaves as the original.
  address_t original_;

  // The address of the replacement function.
  address_t replacement_;

  // Flags that affect how this patch behaves.
  int32_t flags_;

  // The custom code stubs associated with this patch.
  PatchStubs *stubs_;

  // The trampoline function which provides the behavior of the original once
  // it is no longer available through the actual original because it has been
  // patched.
  address_t trampoline_;

  // When the code is open for writing this field holds the previous
  // permissions set on the code.
  dword_t old_perms_;

  // The current status of this patch.
  Status status_;

  // When applied this array holds the code in the original function that was
  // overwritten.
  byte_t overwritten_[kPatchSizeBytes];
};

// Size of the entry component of a patch stub.
#define kEntryPatchStubSizeBytes 32

// Size of the helper component of a patch stub.
#define kHelperPatchStubSizeBytes 32

/// ## Patch stubs
///
/// The patch stubs struct is really just an alias for two blocks of code, the
/// entry stub and the helper stub. Each patch request has one pair of patch
/// stubs associated with it and, when applied, will jump unconditionally to
/// the entry stub.
///
/// To make the initial application of the patch set as cheap as possible we
/// only do a partial application and then complete it when the original
/// function is first called. When first called the entry stub contains code
/// that causes patching to be completed and the helper contains part of the
/// code that accomplishes this. Once patching is complete the entry will
/// contain a jump directly to the replacement function.
struct PatchStubs {
  PatchRequest *origin_;
  byte_t entry_[kEntryPatchStubSizeBytes];
  byte_t helper_[kHelperPatchStubSizeBytes];
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
///      can bail out immediately.
///   2. Then we make the relevant memory regions holding the original code
///      writeable. If this fails part way through we have to revert the
///      permissions before aborting. We still haven't actually changed any
///      code.
///   3. Then we patch the code. It shouldn't be possible for this to fail since
///      the permissions are set up appropriately and the stubs have been
///      validated.
///   4. Finally we revert the permissions on any memory we've written.
///
/// At this point any calls to the patched functions will be redirected to the
/// patch stubs which will complete the patch process on first call.
class PatchSet {
public:
  // Indicates the current state of this patch set.
  enum Status {
    NOT_APPLIED,
    PREPARED,
    APPLIED,
    FAILED
  };

  // Creates a patch set that applies the given list of patches.
  PatchSet(MemoryManager &memman, Vector<PatchRequest> requests);

  // Prepares to apply the patches. Preparing makes no code changes, it only
  // checks whether patching is possible and allocates the data needed to do
  // the patching. Returns true iff preparing succeeded.
  bool prepare_apply();

  // Determines the lower and upper bound of the addresses to patch by scanning
  // over the requests and min/max'ing over the addresses.
  Vector<byte_t> determine_address_range();

  // Returns true if any offset between two addresses that are at most the given
  // distance apart will fit in 32 bits.
  static bool offsets_fits_in_32_bits(size_t dist);

private:
  // The patch engine used to apply this patch set.
  MemoryManager &memman_;
  MemoryManager &memman() { return memman_; }

  // The patch requests to apply.
  Vector<PatchRequest> requests_;
  Vector<PatchRequest> &requests() { return requests_; }

  // The patch stubs that hold the code implementing the requests.
  Vector<PatchStubs> stubs_;

  // The current status.
  Status status_;
};

// Casts a function to an abstract function_t.
#define FUNCAST(EXPR) reinterpret_cast<address_t>(EXPR)

// The platform-specific object that knows how to allocate and manipulate memory.
class MemoryManager {
public:
  virtual ~MemoryManager();

  // If the manager needs any kind of initialization this call will ensure that
  // it has been initialized.
  virtual bool ensure_initialized();

  // Attempts to open the page that holds the given address such that it can be
  // written. If successful the previous permissions should be stored in the
  // given out parameter.
  virtual bool try_open_page_for_writing(address_t addr, dword_t *old_perms) = 0;

  // Attempts to close the currently open page that holds the given address
  // by restoring the permissions to the given value, which was the on stored
  // in the out parameter by the open method.
  virtual bool try_close_page_for_writing(address_t addr, dword_t old_perms) = 0;

  // Allocates a piece of executable memory of the given size near enough to
  // the given address that we can jump between them. If memory can't be
  // successfully allocated returns an empty vector.
  virtual Vector<byte_t> alloc_executable(address_t addr, size_t size) = 0;

  // Returns the memory manager appropriate for this platform.
  static MemoryManager &get();
};

#endif // _BINPATCH
