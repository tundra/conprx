//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// ## Cross-platform code for binary patching.
///
/// A binary patch in this context is one that causes the implementation of one
/// C function to be replaced by a different one without requiring changes to
/// the callers of the initial function. In other words, the function you think
/// you're calling is replaced with a different one under your feet.
///
/// The way this is done is by destructively overwriting the first instructions
/// of the function to be replaced with a jump to the replacement. The process
/// works as follows.
///
/// The function to patch and its replacement are assumed to be no more than
/// 2^32 bytes apart in memory. The first 5 bytes in the original function is
/// replaced with a jump to the replacement. Note that this may not happen
/// cleanly -- the end of the 5 bytes may be in the middle of some instruction
/// but it doesn't matter since we're jumping away from there anyway.
///
/// At the same time a trampoline function is constructed that first executes
/// the instructions that were overwritten in the original, the _preamble_. That
/// means not just the first 5 bytes since, as mentioned above, the 5-byte
/// boundary may fall within an instruction and we need the full instruction to
/// be executed by the trampoline. Then the trampoline jumps to the point in the
/// original function immediately following the preamble at which point
/// everything works as if the original function had been intact.

#ifndef _BINPATCH
#define _BINPATCH

#include "utils/types.hh"

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

// The type of a function to be patched.
typedef void *function_t;

// Casts a function to an abstract function_t.
#define FUNCAST(EXPR) reinterpret_cast<function_t>(EXPR)

// The platform-specific engine that knows how to apply binary patches.
class PatchEngine {
public:
  virtual ~PatchEngine();

  // If the engine needs any kind of initialization this call will ensure that
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
  // successfully allocated returns NULL.
  virtual address_t alloc_executable(address_t addr, size_t size) = 0;

  // Returns the patch engine appropriate for this platform.
  static PatchEngine &get();
};

// Encapsulates an individual binary patch. Binary patches are platform
// independent, - any platform code is encapsulated in the patch engine which
// the patch object will delegate to as appropriate.
class BinaryPatch {
public:
  // Indicates the current state of this patch.
  enum Status {
    // This patch hasn't been applied yet, or it has been applied and then
    // successfully reverted.
    NOT_APPLIED,
    APPLIED,
    FAILED
  };

  // Flags that control how the patch is applied.
  enum Flag {
    MAKE_TRAMPOLINE = 0x1
  };

  // Initializes a binary patch that replaces the given original function with
  // the given replacement.
  BinaryPatch(function_t original, function_t replacement, uint32_t flags = MAKE_TRAMPOLINE);

  // Checks, without making any changes, whether the given request may be
  // possible. This is to catch cases where we can tell early on that a patch
  // couldn't possibly succeed, without necessarily guaranteeing that it will
  // succeed if you try. The function that does the actual patching can assume
  // that this has been called and returned true.
  bool is_tentatively_possible();

  // Returns the current status.
  Status status() { return status_; }

  // Attempts to apply this patch.
  bool apply(PatchEngine &engine);

  // Attempts to revert the patched code to its original state.
  bool revert(PatchEngine &engine);

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
  byte_t *build_trampoline(PatchEngine &engine);

  // Returns the size of the trampoline preamble to use for the trampoline that
  // behaves like the function starting at the given address. If it is not
  // possible to trampoline this returns 0.
  static size_t get_preamble_size(address_t addr);

  // The address of the original function. Note that when the patch has been
  // applied the function at this address will effectively be the replacement,
  // to call the original use the generated trampoline which is a different
  // function but behaves as the original.
  function_t original_;

  // The address of the replacement function.
  function_t replacement_;

  // Flags that affect how this patch behaves.
  int32_t flags_;

  // The trampoline function which provides the behavior of the original once
  // it is no longer available through the actual original because it has been
  // patched.
  function_t trampoline_;

  // When the code is open for writing this field holds the previous
  // permissions set on the code.
  dword_t old_perms_;

  // The current status of this patch.
  Status status_;

  // When applied this array holds the code in the original function that was
  // overwritten.
  byte_t overwritten_[kPatchSizeBytes];
};

#endif // _BINPATCH
