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

class InstructionSet;
class MemoryManager;
class Platform;
struct PatchCode;

// The biggest possible redirect sequence.
#define kMaxRedirectSizeBytes 8

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
  // Initializes a binary patch that replaces the given original function with
  // the given replacement.
  PatchRequest(address_t original, address_t replacement);

  PatchRequest();

  // Marks this request as having been prepared to be applied.
  void preparing_apply(Platform *platform, PatchCode *code);

  // Attempts to apply this patch.
  bool apply(MemoryManager &memman);

  address_t original() { return reinterpret_cast<address_t>(original_); }

  address_t replacement() { return reinterpret_cast<address_t>(replacement_); }

  PatchCode &code() { return *code_; }

  Platform &platform() { return *platform_; }

  // Returns the trampoline code viewed as the given type.
  template <typename T>
  T get_trampoline() { return reinterpret_cast<T>(get_or_create_trampoline()); }

  // Returns the trampoline code viewed as the same type as the given argument.
  // Since function types are so long this can be a convenient shorthand.
  template <typename T>
  T get_trampoline(T prototype) { return get_trampoline<T>(); }

  Vector<byte_t> overwritten() { return Vector<byte_t>(overwritten_, kMaxRedirectSizeBytes); }

private:
  // Returns the address of the trampoline, creating it on first call.
  address_t get_or_create_trampoline();

  // Writes the trampoline code.
  void write_trampoline();

  // The address of the original function. Note that when the patch has been
  // applied the function at this address will effectively be the replacement,
  // to call the original use the generated trampoline which is a different
  // function but behaves as the original.
  address_t original_;

  // The address of the replacement function.
  address_t replacement_;

  // True iff the appropriate code has been written into the trampoline stub.
  bool has_written_trampoline_;

  // The custom code stubs associated with this patch.
  PatchCode *code_;

  // When applied this array holds the code in the original function that was
  // overwritten.
  byte_t overwritten_[kMaxRedirectSizeBytes];

  // The platform utilities.
  Platform *platform_;
};

// Size of the helper component of a patch stub.
#define kTrampolinePatchStubSizeBytes 32

/// ## Patch code
///
/// The patch code struct is a container for the executable code used by a
/// patch. Each patch request has a patch code struct associated with it.
///
/// To make the initial application of the patch set as cheap as possible we
/// initially only redirect and then generate the trampoline on demand. The
/// request keeps track of the state of the code.
struct PatchCode {
  byte_t trampoline_[kTrampolinePatchStubSizeBytes];
};

// All the platform dependent objects bundled together.
class Platform {
public:
  // Ensures that the platform object is fully initialized, if it hasn't been
  // already. Returns true on success.
  bool ensure_initialized();

  MemoryManager &memory_manager() { return memman_; }

  InstructionSet &instruction_set() { return inst_; }

  // Returns the current platform. Remember to initialize it before using it.
  static Platform &get();

private:
  Platform(InstructionSet &inst, MemoryManager &memman);

  InstructionSet &inst_;
  MemoryManager &memman_;
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
  bool prepare_apply();

  // Attempts to set the permissions on the code we're going to patch. Returns
  // true iff this succeeds.
  bool open_for_patching();

  // Attempts to return the permissions on the code that has been patched to
  // the state it was before.
  bool close_after_patching(Status success_status);

  // Patches the redirect instructions into the original functions.
  void install_redirects();

  // Restores the original functions to their initial state.
  void revert_redirects();

  // Given a fresh patch set, runs through the sequence of operations that
  // causes the patches to be applied.
  bool apply();

  // Runs through the complete sequence of operations that restores the
  // originals to their initial state.
  bool revert();

  // Determines the lower and upper bound of the addresses to patch by scanning
  // over the requests and min/max'ing over the addresses. Note that this is
  // not the range we're going to write within but slightly narrower -- it
  // stops at the beginning of the last function to write and we'll be writing
  // a little bit past the beginning. Use determine_patch_range to get the
  // full range we'll be writing.
  Vector<byte_t> determine_address_range();

  // Does basically the same thing as determine_address_range but takes into
  // account that we have to write some amount past the last address. All the
  // memory we'll be writing will be within this range.
  Vector<byte_t> determine_patch_range();

  // Returns true if any offset between two addresses that are at most the given
  // distance apart will fit in 32 bits.
  static bool offsets_fit_in_32_bits(size_t dist);

  Status status() { return status_; }

private:
  // Attempts to write all the locations we'll be patching. Returns false or
  // crashes if writing fails.
  bool validate_open_for_patching();

  // Applies an individual request.
  void install_redirect(PatchRequest &request);

  // The patch engine used to apply this patch set.
  Platform &platform_;
  MemoryManager &memory_manager() { return platform_.memory_manager(); }
  InstructionSet &instruction_set() { return platform_.instruction_set(); }

  // The patch requests to apply.
  Vector<PatchRequest> requests_;
  Vector<PatchRequest> &requests() { return requests_; }

  // The patch stubs that hold the code implementing the requests.
  Vector<PatchCode> codes_;

  // The current status.
  Status status_;

  // When the code is open for writing this field holds the previous
  // permissions set on the code.
  dword_t old_perms_;
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
class MemoryManager {
public:
  virtual ~MemoryManager();

  // If the manager needs any kind of initialization this call will ensure that
  // it has been initialized.
  virtual bool ensure_initialized();

  // Attempts to open the given memory region such that it can be written. If
  // successful the previous permissions should be stored in the given out
  // parameter.
  virtual bool open_for_writing(Vector<byte_t> region, dword_t *old_perms) = 0;

  // Attempts to close the given memory region such that it can no longer be
  // written. The old_perms parameter contains the value that was stored by
  // open_for_writing.
  virtual bool close_for_writing(Vector<byte_t> region, dword_t old_perms) = 0;

  // Allocates a piece of executable memory of the given size near enough to
  // the given address that we can jump between them. If memory can't be
  // successfully allocated returns an empty vector.
  virtual Vector<byte_t> alloc_executable(address_t addr, size_t size) = 0;

  // Returns the memory manager appropriate for this platform.
  static MemoryManager &get();
};

// An instruction set encapsulates the flavor of machine code used on the
// current platform.
class InstructionSet {
public:
  virtual ~InstructionSet();

  // Returns the size in bytes of the code patch that redirects execution from
  // the original code to the replacement.
  virtual size_t get_redirect_size_bytes() = 0;

  // Installs a redirect from the request's original function to its entry
  // stub.
  virtual void install_redirect(PatchRequest &request) = 0;

  // Writes trampoline code into the given code object that implements the same
  // behavior as the request's original function did before it was replaced.
  virtual void write_trampoline(PatchRequest &request, PatchCode &code) = 0;

  // Returns the instruction set to use on this platform.
  static InstructionSet &get();
};

#endif // _BINPATCH
