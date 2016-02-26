//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _BINPATCH_X86
#define _BINPATCH_X86

#include "disass/disassembler-x86.hh"
#include "utils/alloc.hh"
#include "utils/vector.hh"

namespace conprx {

// Holds functionality shared between variants of x86.
class GenericX86 : public InstructionSet {
public:
  // Strategy that jumps to a location using a relative 32-bit jump.
  class RelativeJump32Redirection : public Redirection {
  public:
    virtual void default_destroy() { default_delete_concrete(this); }
    virtual size_t write_redirect(address_t code, address_t dest);
  };

  // Yields the appropriate disassembler for this architecture.
  virtual Disassembler *disassembler() = 0;

  virtual fat_bool_t prepare_patch(address_t original, address_t replacement,
        tclib::pass_def_ref_t<Redirection> *redir_out, PreambleInfo *info_out);

  // Returns the ideal preamble size to try to make available. What actually
  // gets patched may be different, either because not enough preamble is
  // available or what is available isn't aligned with the optimal size, but
  // this is what we aim for.
  virtual size_t optimal_preable_size() = 0;

  virtual void flush_instruction_cache(tclib::Blob memory);

  virtual void write_halt(tclib::Blob memory);

  // Returns true iff it is possible to jump from the given from-address to the
  // given to-address with a 32-bit relative jump, the from address being the
  // beginning of the jump instruction.
  static bool can_jump_relative_32(address_t from, address_t to);

protected:
  static const byte_t kInt3 = 0xcc;
  static const byte_t kJmp = 0xe9;
  static const size_t kJmpSize = 5;
  static const size_t kRedirectSizeBytes = kJmpSize;

  // Write a relative 32-bit jump at the given location to the given
  // destination.
  static size_t write_relative_jump_32(address_t code, address_t dest);

  virtual fat_bool_t create_redirection(address_t original, address_t replacement,
      tclib::pass_def_ref_t<Redirection> *redir_out, PreambleInfo *info) = 0;
};

} // namespace conprx

#endif // _BINPATCH_X86
