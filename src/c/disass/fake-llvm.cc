//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "fake-llvm-inl.hh"

using namespace llvm;

raw_ostream raw_ostream::kInstance;

uint64_t MCSubtargetInfo::getFeatureBits() const {
  return 0;
}

MCDisassembler::MCDisassembler(const MCSubtargetInfo &) : CommentStream(NULL) { }

MCDisassembler::~MCDisassembler() { }

bool MCDisassembler::tryAddingSymbolicOperand(MCInst&, int64_t, uint64_t, bool,
    uint64_t, uint64_t) const {
  return true;
}

void MCDisassembler::tryAddingPcLoadReferenceComment(int64_t, uint64_t) const {
  // ignore
}

MCInstrInfo *Target::createMCInstrInfo() const {
  return NULL;
}

Target llvm::TheX86_32Target;
Target llvm::TheX86_64Target;

void TargetRegistry::RegisterMCDisassembler(const Target&,
    MCDisassembler *(*)(const Target&, const MCSubtargetInfo&)) {
  // do nothing
}

MemoryObject::MemoryObject(Vector<byte_t> memory) : memory_(memory) { }

uint64_t MemoryObject::getBase() const {
  return reinterpret_cast<uint64_t>(memory_.start());
}

uint64_t MemoryObject::getExtent() const {
  return memory_.length();
}

int MemoryObject::readByte(uint64_t address, uint8_t *ptr) const {
  if (memory_.length() <= address) {
    return -1;
  } else {
    *ptr = memory_[static_cast<size_t>(address)];
    return 0;
  }
}
