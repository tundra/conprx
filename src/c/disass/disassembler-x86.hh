//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _DISASSEMBLER_X86
#define _DISASSEMBLER_X86

#include "utils/vector.hh"

namespace conprx {

// Wrapper around an llvm disassembler.
class Disassembler {
public:
  virtual ~Disassembler();

  enum Status {
    INVALID_INSTRUCTION,
    RESOLVED
  };

  // Information about a machine instruction.
  struct resolve_result {
    size_t length;
    byte_t opcode;
  };

  // Returns the length of the instruction starting at the given offset within
  // the given block of code.
  virtual Status resolve(Vector<byte_t> code, size_t offset, resolve_result *result_out) = 0;

  // Returns the singleton X86-64 disassembler.
  static Disassembler &x86_64();
};

} // namespace conprx

#endif // _DISASSEMBLER_X86
