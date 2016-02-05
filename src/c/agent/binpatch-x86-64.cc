//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/binpatch-x86.hh"
#include "disass/disassembler-x86.hh"
#include "utils/log.hh"
#include "utils/types.hh"

using namespace conprx;

class X86_64 : public GenericX86 {
public:
  class AbsoluteJump64Redirection : public Redirection {
  public:
    virtual size_t write_redirect(address_t code, address_t dest);
  };

  virtual size_t write_imposter(PatchRequest &request, tclib::Blob memory);
  virtual Disassembler *disassembler();
  virtual size_t optimal_preable_size() { return kAbsoluteJump64Size; }
  virtual Redirection *create_redirection(address_t original, address_t replacement,
      PreambleInfo *info);

  // Returns the singleton ia32 instance.
  static X86_64 &get();


private:
  static const byte_t kRexWB = 0x49;
  static const byte_t kMovR11 = 0xbb;
  static const byte_t kRexB = 0x41;
  static const byte_t kJmpQ = 0xff;
  static const byte_t kRmR11 = 0xe3;

  static const size_t kAbsoluteJump64Size = 13;

  static size_t write_absolute_jump_64(address_t code, address_t dest);
};

size_t X86_64::AbsoluteJump64Redirection::write_redirect(address_t code, address_t dest) {
  return write_absolute_jump_64(code, dest);
}

size_t X86_64::write_absolute_jump_64(address_t code, address_t dest) {
  size_t offset = 0;
  // Store the address to jump to i %r11 which, according to all ABIs I've seen,
  // a callee-save register so it should be safe to clobber.
  code[offset++] = kRexWB;
  code[offset++] = kMovR11;
  *reinterpret_cast<uint64_t*>(code + offset) = reinterpret_cast<uint64_t>(dest);
  offset += sizeof(uint64_t);
  // Then long-jump to the contents of %r11.
  code[offset++] = kRexB;
  code[offset++] = kJmpQ;
  code[offset++] = kRmR11;
  return kAbsoluteJump64Size;
}

Redirection *X86_64::create_redirection(address_t original, address_t replacement,
    PreambleInfo *info) {
  if (info->size() >= kAbsoluteJump64Size) {
    return new AbsoluteJump64Redirection();
  } else if (info->size() < kJmpSize) {
    LOG_WARN("Not enough room to create redirection (0x%02x): %i",
        info->last_instr(), info->size());
    return NULL;
  } else if (can_jump_relative_32(original, replacement)) {
    return new RelativeJump32Redirection();
  } else {
    return NULL;
  }
}

size_t X86_64::write_imposter(PatchRequest &request, tclib::Blob memory) {
  // Initially let the trampoline interrupt (int3) when called. Just in case
  // anyone should decide to call it in the case that we failed below.
  address_t trampoline = static_cast<address_t>(memory.start());
  trampoline[0] = kInt3;
  // Copy the overwritten bytes into the trampoline. We'll definitely have to
  // execute those.
  tclib::Blob preamble_copy = request.preamble_copy();
  memcpy(trampoline, preamble_copy.start(), preamble_copy.size());
  // Then jump back to the original.
  return write_absolute_jump_64(trampoline + preamble_copy.size(),
      request.original() + preamble_copy.size());
}

Disassembler *X86_64::disassembler() {
  return &Disassembler::x86_64();
}

X86_64 &X86_64::get() {
  static X86_64 *instance = NULL;
  if (instance == NULL)
    instance = new X86_64();
  return *instance;
}

InstructionSet &InstructionSet::get() {
  return X86_64::get();
}

#include "binpatch-x86.cc"
