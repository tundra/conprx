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
  // Yields the appropriate disassembler for this architecture.
  virtual Disassembler *disassembler() = 0;

  virtual bool prepare_patch(address_t original, address_t replacement,
      address_t trampoline, size_t *size_out, MessageSink *messages);

  // Validate that we can patch the given code locations using this instruction
  // set. Returns false on failure and will have written a message to the given
  // message sink describing why.
  virtual bool validate_code_locations(address_t original, address_t replacement,
      address_t trampoline, MessageSink *messages) = 0;

protected:
  static const byte_t kInt3 = 0xcc;

};

} // namespace conprx

#endif // _BINPATCH_X86
