//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _DISASSEMBLER_X86
#define _DISASSEMBLER_X86

#include "utils/vector.hh"

namespace conprx {

// Information about a resolved machine instruction, or if resolution fails
// about the failure.
class InstructionInfo {
public:
  // Status.
  enum Status {
    INVALID_INSTRUCTION,
    BLACKLISTED,
    RESOLVED,
    NONE
  };

  Status status() { return status_; }

  // The length of the instruction. This only has a meaningful value if this
  // is the result of a successful call to resolve.
  size_t length() { return length_; }

  // If resolution fails because of a blacklisted instruction this will return
  // the offending instruction.
  byte_t instruction() { return instr_; }

  // Sets the state of this info to be the specified failure. Returns false such
  // that it can be returned as the result of a resolve call.
  bool fail(Status status, byte_t instr);

  // Sets the state of this info to be a success of the given length. Returns
  // true such that it can be returned as the result of a resolve call.
  bool succeed(size_t length);

private:
  Status status_;
  size_t length_;
  byte_t instr_;
};

// Wrapper around an llvm disassembler.
class Disassembler {
public:
  virtual ~Disassembler();

  // Looks up information about the instruction starting at the given offset
  // within the given block of code. Returns true if resolution fails, otherwise
  // true. Information relevant to the resolution is stored in the out
  // parameter.
  virtual bool resolve(Vector<byte_t> code, size_t offset, InstructionInfo *info_out) = 0;

  // Returns the singleton X86-64 disassembler.
  static Disassembler &x86_64();
};

} // namespace conprx

#endif // _DISASSEMBLER_X86
