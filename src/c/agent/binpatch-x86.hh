//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _BINPATCH_X86
#define _BINPATCH_X86

#include "utils/vector.hh"
#include "disass/disassembler-x86.hh"

namespace conprx {


// Holds functionality shared between variants of x86.
class GenericX86 : public InstructionSet {
public:
  // Strategy that jumps to a location using a relative 32-bit jump.
  class RelativeJump32Redirection : public Redirection {
  public:
    virtual size_t write_redirect(address_t code, address_t dest);
  };

  // Yields the appropriate disassembler for this architecture.
  virtual Disassembler *disassembler() = 0;

  virtual Redirection *prepare_patch(address_t original, address_t replacement,
      PreambleInfo *info_out, MessageSink *messages);

  // Returns the ideal preamble size to try to make available. What actually
  // gets patched may be different, either because not enough preamble is
  // available or what is available isn't aligned with the optimal size, but
  // this is what we aim for.
  virtual size_t optimal_preable_size() = 0;

  virtual void flush_instruction_cache(tclib::Blob memory);

  virtual void write_halt(tclib::Blob memory);

protected:
  static const byte_t kInt3 = 0xcc;
  static const byte_t kJmp = 0xe9;
  static const size_t kJmpSize = 5;
  static const size_t kRedirectSizeBytes = kJmpSize;

  // Write a relative 32-bit jump at the given location to the given
  // destination.
  static size_t write_relative_jump_32(address_t code, address_t dest);

  virtual Redirection *create_redirection(address_t original, address_t replacement,
      PreambleInfo *info, MessageSink *messages) = 0;
};

} // namespace conprx

#endif // _BINPATCH_X86
