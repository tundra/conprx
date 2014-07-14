//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// This is a wrapper around the LLVM disassembler code. The goal is to keep
/// changes to that code minimal so to the extent possible we effect changes by
/// defining stuff here and then source-including their source.

#include "utils/log.hh"
#include "disassembler-x86.hh"
#include "utils/types.hh"
#include "fake-llvm-inl.hh"

// MSVC has a few issues with the code below but luckily only warnings so we can
// silence those.
#ifdef IS_MSVC
#pragma warning(push)
#pragma warning(disable : 4530 4986)
#endif

#include <assert.h>
#include <string>

// We need this bit of setup for the headers to compile. Found by trial and
// error.
#define INSTRUCTION_SPECIFIER_FIELDS uint16_t operands;
#define INSTRUCTION_IDS uint16_t instructionIDs;
#define NDEBUG 1

// The BOOL defined below will clash with the one from windows.h if we're not
// careful.
#ifdef IS_MSVC
#define BOOL DONT_CLASH_WITH_WINDEF_BOOL
#endif

// Include the headers required by the disassembler. The import script strips
// includes from within the files it imports so all the required imports have
// to be given explicitly here, in dependency order.
#include "llvm/MCInst.hh"
#include "llvm/MCRegisterInfo.hh"
#include "llvm/MCInstrDesc.hh"
#include "llvm/MCInstrInfo.hh"
#include "llvm/X86DisassemblerDecoderCommon.h"
#include "llvm/X86DisassemblerDecoder.h"
#include "llvm/X86Disassembler.h"

// Include the generated files.
#define GET_REGINFO_ENUM
#include "llvm/X86GenRegisterInfo.inc"
#define GET_INSTRINFO_ENUM
#include "llvm/X86GenInstrInfo.inc"

// Finally, include the disassembler implementation itself. We include it here
// both becuase it needs all the setup above but also because we need to pick
// out some of the internals of it below.
#include "llvm/X86Disassembler.cc"

using namespace conprx;

bool InstructionInfo::fail(Status status, byte_t instr) {
  length_ = 0;
  status_ = status;
  instr_ = instr;
  return false;
}

bool InstructionInfo::succeed(size_t length) {
  length_ = length;
  status_ = RESOLVED;
  instr_ = 0;
  return true;
}

Disassembler::~Disassembler() { }

// An x86-64 disassembler implemented by calling through to the llvm
// disassembler.
class Dis_X86_64 : public Disassembler {
public:
  Dis_X86_64();
  virtual bool resolve(Vector<byte_t> code, size_t offset,
      InstructionInfo *info_out);

  // Returns true iff the given instruction is in the instruction whitelist.
  // The whitelist exists because we can't safely copy any instruction -- for
  // instance, relative jumps have to be adjusted and returns can signify the
  // end of the code. So to be on the safe side we'll refuse to copy
  // instructions not explicitly whitelisted.
  bool in_whitelist(byte_t instr);

private:
  // Adaptor that allows the decoder, which is implemented in C, to read from
  // vectors.
  static int vector_byte_reader(const void* arg, uint8_t* byte,
      uint64_t address);

  // Bools that signify whether a value is in the whitelist or not.
  bool in_whitelist_[256];

  // A list of opcodes we're willing to patch.
  static const byte_t kOpcodeWhitelist[];
  static const size_t kOpcodeWhitelistSize = 31;

};

const byte_t Dis_X86_64::kOpcodeWhitelist[kOpcodeWhitelistSize] = {
  0x00, // nop
  0x01, // add
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, // push reg
  0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, // pop reg
  0x68, // push imm16/32
  0x6a, // push imm8
  0x89, // mov
  0x8b, // mov
  0x8d, // lea
  0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf // mov reg
};

Dis_X86_64::Dis_X86_64() {
  for (size_t i = 0; i < 256; i++)
    in_whitelist_[i] = false;
  for (size_t i = 0; i < kOpcodeWhitelistSize; i++)
    in_whitelist_[kOpcodeWhitelist[i]] = true;
}

bool Dis_X86_64::in_whitelist(byte_t opcode) {
  return in_whitelist_[opcode];
}

Disassembler &Disassembler::x86_64() {
  static Dis_X86_64 *instance = NULL;
  if (instance == NULL)
    instance = new Dis_X86_64();
  return *instance;
}

int Dis_X86_64::vector_byte_reader(const void* arg, uint8_t* byte,
    uint64_t address) {
  const Vector<byte_t> &vect = *static_cast<const Vector<byte_t>*>(arg);
  if (address >= vect.length()) {
    return -1;
  } else {
    *byte = vect[address];
    return 0;
  }
}

bool Dis_X86_64::resolve(Vector<byte_t> code, size_t offset,
    InstructionInfo *info_out) {
  struct InternalInstruction instr;
  int ret = decodeInstruction(&instr, vector_byte_reader, &code, NULL, NULL,
      NULL, offset, MODE_64BIT);
  if (ret)
    // The return value is 0 on success.
    return info_out->fail(InstructionInfo::INVALID_INSTRUCTION, 0);
  if (!in_whitelist(instr.opcode))
    return info_out->fail(InstructionInfo::BLACKLISTED, instr.opcode);
  return info_out->succeed(instr.readerCursor - offset);
}

#ifdef IS_MSVC
#pragma warning(pop)
#endif
