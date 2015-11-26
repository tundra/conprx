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
#pragma warning(disable : 4530 4986 4267)
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

// Disable all these warnings for included files, there's nothing we can do to
// fix them anyway.
#ifdef IS_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wc++-compat"
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

#ifdef IS_GCC
#  pragma GCC diagnostic pop
#endif

using namespace conprx;

bool InstructionInfo::fail(Status status, byte_t instr) {
  length_ = 0;
  status_ = status;
  instr_ = instr;
  return false;
}

bool InstructionInfo::succeed(size_t length, bool at_end, byte_t instr) {
  length_ = length;
  status_ = at_end ? RESOLVED_END : RESOLVED;
  instr_ = instr;
  return true;
}

Disassembler::~Disassembler() { }

// Whitelisted for both 32 and 64 bits.
static const byte_t kX86_WhitelistSize = 50;
static const byte_t kX86_Whitelist[kX86_WhitelistSize] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, // add
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, // push reg
  0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, // pop reg
  0x88, 0x89, 0x8a, 0x8b, 0x8c, // mov
  0x8d, // lea
  0x90, // nop
  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, // mov imm8
  0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, // mov reg
};

// Whitelisted just for 64 bits.
static const byte_t kX86_64WhitelistSize = 6;
static const byte_t kX86_64Whitelist[kX86_64WhitelistSize] = {
  0x33, // xor
  0x68, // push imm16/32
  0x6a, // push imm8
  0x81, // logic (add, or, etc) imm16/32
  0x83, // logic (add, or, etc) imm8
  0xff, // near absolute call
};

static const byte_t kX86_EndsSize = 2;
static const byte_t kX86_Ends[kX86_EndsSize] = {
  0xc3, // ret
  0xcc, // int3
};

Disassembler::Disassembler(int32_t mode)
  : mode_(mode) {
  for (size_t i = 0; i < 256; i++)
    types_[i] = NONE;
}

void Disassembler::set_types(Vector<const byte_t> instrs, InstructionType type) {
  for (size_t i = 0; i < instrs.length(); i++)
    types_[instrs[i]] = type;
}

bool Disassembler::in_whitelist(byte_t opcode) {
  return types_[opcode] != NONE;
}

bool Disassembler::is_end(byte_t opcode) {
  return types_[opcode] == WHITELISTED_END;
}

int Disassembler::vector_byte_reader(const void* arg, uint8_t* byte,
    uint64_t address) {
  const Vector<byte_t> &vect = *static_cast<const Vector<byte_t>*>(arg);
  if (address >= vect.length()) {
    return -1;
  } else {
    *byte = vect[static_cast<size_t>(address)];
    return 0;
  }
}

bool Disassembler::resolve(Vector<byte_t> code, size_t offset,
    InstructionInfo *info_out) {
  struct InternalInstruction instr;
  int ret = decodeInstruction(&instr, vector_byte_reader, &code, NULL, NULL,
      NULL, offset, static_cast<DisassemblerMode>(mode_));
  if (ret)
    // The return value is 0 on success.
    return info_out->fail(InstructionInfo::INVALID_INSTRUCTION, 0);
  if (!in_whitelist(instr.opcode))
    return info_out->fail(InstructionInfo::NOT_WHITELISTED, instr.opcode);
  bool at_end = is_end(instr.opcode);
  return info_out->succeed(static_cast<size_t>(instr.readerCursor - offset),
      at_end, instr.opcode);
}

Disassembler &Disassembler::x86_64() {
  static Disassembler *instance = NULL;
  if (instance == NULL) {
    instance = new Disassembler(MODE_64BIT);
    Vector<const byte_t> x86_32_wl(kX86_Whitelist, kX86_WhitelistSize);
    instance->set_types(x86_32_wl, Disassembler::WHITELISTED);
    Vector<const byte_t> x86_64_wl(kX86_64Whitelist, kX86_64WhitelistSize);
    instance->set_types(x86_64_wl, Disassembler::WHITELISTED);
    Vector<const byte_t> x86_ends(kX86_Ends, kX86_EndsSize);
    instance->set_types(x86_ends, Disassembler::WHITELISTED_END);
  }
  return *instance;
}

Disassembler &Disassembler::x86_32() {
  static Disassembler *instance = NULL;
  if (instance == NULL) {
    instance = new Disassembler(MODE_32BIT);
    Vector<const byte_t> x86_32_wl(kX86_Whitelist, kX86_WhitelistSize);
    instance->set_types(x86_32_wl, Disassembler::WHITELISTED);
    Vector<const byte_t> x86_ends(kX86_Ends, kX86_EndsSize);
    instance->set_types(x86_ends, Disassembler::WHITELISTED_END);
  }
  return *instance;
}

#ifdef IS_MSVC
#pragma warning(pop)
#endif
