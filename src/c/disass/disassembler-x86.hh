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
  enum Status {
    // This instruction was garbage.
    INVALID,
    // We decoded the instruction but don't know how to deal with it.
    UNKNOWN,
    // Decoded the instruction and we can skip over it no problem.
    BENIGN,
    // Decoded the instruction and we can skip over it but not longer.
    TERMINATOR,
    // Decoded but we can't move this instruction, it needs to be left alone.
    SENSITIVE
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
  bool succeed(size_t length, byte_t instr, Status status);

private:
  Status status_;
  size_t length_;
  byte_t instr_;
};

// Wrapper around an llvm disassembler.
class Disassembler {
public:
  enum InstructionType {
    // This instruction has no particular type associated with it.
    NONE = 0x0,
    // We don't allow this instruction to be patched
    BLACKLISTED = 0x1,
    // We allow this instruction to be patched over.
    WHITELISTED = 0x2,
    // This instruction is whitelisted but we don't allow any instructions to
    // be patched after this point.
    TERMINATOR = 0x3,
  };

  Disassembler(int32_t mode);
  virtual ~Disassembler();

  // Set the type of the given instructions to the given value.
  void set_types(Vector<const byte_t> instrs, InstructionType type);

  // Looks up information about the instruction starting at the given offset
  // within the given block of code. Returns true if resolution fails, otherwise
  // true. Information relevant to the resolution is stored in the out
  // parameter.
  virtual bool resolve(Vector<byte_t> code, size_t offset, InstructionInfo *info_out);

  // Returns true iff the given instruction is in the instruction whitelist.
  // The whitelist exists because we can't safely copy any instruction -- for
  // instance, relative jumps have to be adjusted and returns can signify the
  // end of the code. So to be on the safe side we'll refuse to copy
  // instructions not explicitly whitelisted.
  bool in_whitelist(byte_t instr);

  bool in_blacklist(byte_t instr);

  bool is_terminator(byte_t instr);

  // Returns the singleton X86-64 disassembler.
  static Disassembler &x86_64();

  // Returns the singleton X86-32 disassembler.
  static Disassembler &x86_32();

private:
  // Adaptor that allows the decoder, which is implemented in C, to read from
  // vectors.
  static int vector_byte_reader(const void* arg, uint8_t* byte,
      uint64_t address);

  int32_t mode_;

  // Bools that signify whether a value is in the whitelist or not.
  InstructionType types_[256];
};

} // namespace conprx

#endif // _DISASSEMBLER_X86
