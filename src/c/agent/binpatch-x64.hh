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

#ifndef _BINPATCH_X64
#define _BINPATCH_X64

#include "utils/types.hh"
#include "utils/vector.hh"

namespace conprx {

class X64 : public InstructionSet {
public:
  virtual size_t get_redirect_size_bytes();
  virtual void install_redirect(PatchRequest &request);
  virtual bool write_trampoline(PatchRequest &request, PatchCode &code);

  // Returns the singleton ia32 instance.
  static X64 &get();

private:
  static const byte_t kJmpOp = 0xE9;
  static const size_t kJmpOpSizeBytes = 5;
  static const byte_t kInt3Op = 0xCC;

  static const size_t kRedirectSizeBytes = kJmpOpSizeBytes;
};

} // namespace conprx

#endif // _BINPATCH_X64
