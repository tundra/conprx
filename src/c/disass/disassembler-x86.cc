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
#include <math.h>
#include <string>

using namespace conprx;

// We need this bit of setup for the headers to compile. Found by trial and
// error.
#define INSTRUCTION_SPECIFIER_FIELDS uint16_t operands;
#define INSTRUCTION_IDS uint16_t instructionIDs;
#define NDEBUG 1

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

Disassembler::~Disassembler() { }

// An x86-64 disassembler implemented by calling through to the llvm
// disassembler.
class Dis_X86_64 : public Disassembler {
public:
  Dis_X86_64();
  virtual Status resolve(Vector<byte_t> code, size_t offset, resolve_result *result_out);

private:
  llvm::MCSubtargetInfo subtarget_;
  llvm::MCDisassembler *disass_;
};

Dis_X86_64::Dis_X86_64() {
  disass_ = createX86_64Disassembler(llvm::TheX86_64Target, subtarget_);
}

Disassembler &Disassembler::x86_64() {
  static Dis_X86_64 *instance = NULL;
  if (instance == NULL)
    instance = new Dis_X86_64();
  return *instance;
}

Disassembler::Status Dis_X86_64::resolve(Vector<byte_t> code, size_t offset,
    resolve_result *result_out) {
  llvm::MCInst instr;
  uint64_t size = 0;
  llvm::MemoryObject memory(code);
  MCDisassembler::DecodeStatus decode_status = disass_->getInstruction(instr,
      size, memory, offset, llvm::nulls(), llvm::nulls());
  if (decode_status != MCDisassembler::Success)
    return Disassembler::INVALID_INSTRUCTION;
  result_out->length = size;
  result_out->opcode = instr.getOpcode();
  return Disassembler::RESOLVED;
}

#ifdef IS_MSVC
#pragma warning(pop)
#endif
