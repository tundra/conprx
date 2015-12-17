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

#include "c/stdvector.hh"
#include "utils/blob.hh"
#include "utils/vector.hh"

BEGIN_C_INCLUDES
#include "utils/string.h"
END_C_INCLUDES

namespace conprx {

class InstructionSet;
class MemoryManager;
class Platform;
class TrampolineCode;

// The biggest possible redirect sequence.
#define kMaxPreambleSizeBytes 16

// This is just another name for a dword_t but can be used without including the
// full nightmare that is windows.h.
typedef uint32_t standalone_dword_t;

#define REPORT_MESSAGE(sink, ...) (sink)->report(__FILE__, __LINE__, __VA_ARGS__)

class MessageSink {
public:
  virtual ~MessageSink() { }

  // Record that an error occurred and return false, such that you can always
  // fail an operation by doing,
  //
  //   return REPORT_MESSAGE(messages, ...);
  bool report(const char *file, int line, const char *fmt, ...);

protected:
  // Handle the given message appropriately for this sink. The message is owned
  // by the caller so if you want to hang on to it you need to dup it
  // explicitly.
  //
  // The default implementation logs the message.
  virtual void handle_message(utf8_t message);
};

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
  PatchRequest(address_t original, address_t replacement, const char *name = NULL);

  PatchRequest();

  // Marks this request as having been prepared to be applied.
  bool prepare_apply(Platform *platform, TrampolineCode *code, MessageSink *messages);

  // Attempts to apply this patch.
  bool apply(MemoryManager &memman);

  address_t original() { return reinterpret_cast<address_t>(original_); }

  address_t replacement() { return reinterpret_cast<address_t>(replacement_); }

  size_t preamble_size() { return preamble_size_; }

  TrampolineCode *trampoline_code() { return trampoline_code_; }

  Platform &platform() { return *platform_; }

  // Returns the trampoline code viewed as the given type.
  template <typename T>
  T get_trampoline() { return reinterpret_cast<T>(get_or_create_trampoline()); }

  // Returns the trampoline code viewed as the same type as the given argument.
  // Since function types are so long this can be a convenient shorthand.
  template <typename T>
  T get_trampoline(T prototype) { return get_trampoline<T>(); }

  Vector<byte_t> preamble() { return Vector<byte_t>(preamble_, preamble_size_); }

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

  const char *name_;

  // The completed trampoline. Will be null until the trampoline has been
  // written.
  address_t trampoline_;

  // The custom code stubs associated with this patch.
  TrampolineCode *trampoline_code_;

  // A copy of the original method's preamble which we'll overwrite later on.
  byte_t preamble_[kMaxPreambleSizeBytes];

  // The platform utilities.
  Platform *platform_;

  // The length of the original's preamble.
  size_t preamble_size_;
};

// Size of the helper component of a patch stub.
#define kTrampolineCodeStubSizeBytes 32

/// ## Patch code
///
/// The patch code struct is a container for the executable code used by a
/// patch. Each patch request has a patch code struct associated with it.
///
/// To make the initial application of the patch set as cheap as possible we
/// initially only redirect and then generate the trampoline on demand. The
/// request keeps track of the state of the code.
class TrampolineCode {
public:
  tclib::Blob memory() { return tclib::Blob(stub_, kTrampolineCodeStubSizeBytes); }

private:
  byte_t stub_[kTrampolineCodeStubSizeBytes];
};

// All the platform dependent objects bundled together.
class Platform {
public:
  // Ensures that the platform object is fully initialized, if it hasn't been
  // already. Returns true on success.
  bool ensure_initialized(MessageSink *messages);

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
  bool prepare_apply(MessageSink *messages);

  // Attempts to set the permissions on the code we're going to patch. Returns
  // true iff this succeeds.
  bool open_for_patching(MessageSink *messages);

  // Attempts to return the permissions on the code that has been patched to
  // the state it was before.
  bool close_after_patching(Status success_status, MessageSink *messages);

  // Patches the redirect instructions into the original functions.
  void install_redirects();

  // Restores the original functions to their initial state.
  void revert_redirects();

  // Given a fresh patch set, runs through the sequence of operations that
  // causes the patches to be applied.
  bool apply(MessageSink *messages);

  // Runs through the complete sequence of operations that restores the
  // originals to their initial state.
  bool revert(MessageSink *messages);

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

  Status status() { return status_; }

private:
  // Attempts to write all the locations we'll be patching. Returns false or
  // crashes if writing fails.
  bool validate_open_for_patching(MessageSink *messages);

  // The patch engine used to apply this patch set.
  Platform &platform_;
  MemoryManager &memory_manager() { return platform_.memory_manager(); }
  InstructionSet &instruction_set() { return platform_.instruction_set(); }

  // The patch requests to apply.
  Vector<PatchRequest> requests_;
  Vector<PatchRequest> &requests() { return requests_; }

  // The patch stubs that hold the code implementing the requests.
  Vector<TrampolineCode> codes_;

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
class MemoryManager {
public:
  virtual ~MemoryManager();

  // If the manager needs any kind of initialization this call will ensure that
  // it has been initialized.
  virtual bool ensure_initialized(MessageSink *messages);

  // Attempts to open the given memory region such that it can be written. If
  // successful the previous permissions should be stored in the given out
  // parameter.
  virtual bool open_for_writing(Vector<byte_t> region, standalone_dword_t *old_perms,
      MessageSink *messages) = 0;

  // Attempts to close the given memory region such that it can no longer be
  // written. The old_perms parameter contains the value that was stored by
  // open_for_writing.
  virtual bool close_for_writing(Vector<byte_t> region, standalone_dword_t old_perms,
      MessageSink *messages) = 0;

  // Allocates a piece of executable memory of the given size near enough to
  // the given address that we can jump between them. If memory can't be
  // successfully allocated returns an empty vector.
  virtual Vector<byte_t> alloc_executable(address_t addr, size_t size,
      MessageSink *messages) = 0;

  // Returns the memory manager appropriate for this platform.
  static MemoryManager &get();
};

// An instruction set encapsulates the flavor of machine code used on the
// current platform.
class InstructionSet {
public:
  virtual ~InstructionSet() { }

  // Returns the size in bytes of the code patch that redirects execution from
  // the original code to the replacement.
  virtual size_t redirect_size_bytes() = 0;

  // Performs any preprocessing to be done before patching, including
  // calculating the size of the preamble of the original function that will be
  // overwritten by the redirect. Returns true if successful, false if there is
  // some reason the function can't be patched; if it returns false a message
  // will have been reported to the given message sink.
  virtual bool prepare_patch(address_t original, address_t replacement,
      address_t trampoline, size_t min_size_required, size_t *size_out,
      MessageSink *messages) = 0;

  // Installs a redirect from the request's original function to its entry
  // stub.
  virtual void install_redirect(PatchRequest &request) = 0;

  // Writes trampoline code into the given code object that implements the same
  // behavior as the request's original function did before it was replaced.
  virtual void write_trampoline(PatchRequest &request, tclib::Blob memory) = 0;

  // Notify the processor that there have been code changes.
  virtual void flush_instruction_cache(tclib::Blob memory) = 0;

  // Returns the instruction set to use on this platform.
  static InstructionSet &get();
};

} // namespace conprx

#endif // _BINPATCH
