//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/binpatch-x86.hh"
#include "disass/disassembler-x86.hh"
#include "utils/types.hh"

using namespace conprx;

class X86_64 : public GenericX86 {
public:
  virtual size_t redirect_size_bytes();
  virtual void install_redirect(PatchRequest &request);
  virtual void write_trampoline(PatchRequest &request, PatchCode &code);
  virtual Disassembler *disassembler();
  virtual bool validate_code_locations(address_t original, address_t replacement,
      address_t trampoline, MessageSink *messages);

  // Returns the singleton ia32 instance.
  static X86_64 &get();

private:
  static void write_absolute_jump_64(address_t code, address_t dest);

  static const byte_t kRexWB = 0x49;
  static const byte_t kMovR11 = 0xbb;
  static const byte_t kRexB = 0x41;
  static const byte_t kJmpQ = 0xff;
  static const byte_t kRmR11 = 0xe3;

  static const size_t kRedirectSizeBytes = 13;
};

size_t X86_64::redirect_size_bytes() {
  return kRedirectSizeBytes;
}

bool X86_64::validate_code_locations(address_t original, address_t replacement,
      address_t trampoline, MessageSink *messages) {
  // On x86-64 we can jump anywhere from anywhere.
  return true;
}

void X86_64::install_redirect(PatchRequest &request) {
  address_t original = request.original();
  address_t replacement = request.replacement();

  // Write the redirect to the entry stub into the original.
  write_absolute_jump_64(original, replacement);

  // The redirect may partially overwrite the last instruction, leaving behind
  // invalid code. To avoid that we fill the last bit with one-byte
  // instructions (int3).
  if (kRedirectSizeBytes < request.preamble_size())
    memset(original + kRedirectSizeBytes, kInt3, request.preamble_size() - kRedirectSizeBytes);
}

void X86_64::write_absolute_jump_64(address_t code, address_t dest) {
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
}

void X86_64::write_trampoline(PatchRequest &request, PatchCode &code) {
  // Initially let the trampoline interrupt (int3) when called. Just in case
  // anyone should decide to call it in the case that we failed below.
  address_t trampoline = code.trampoline_;
  trampoline[0] = kInt3;
  // Copy the overwritten bytes into the trampoline. We'll definitely have to
  // execute those.
  Vector<byte_t> preamble = request.preamble();
  memcpy(trampoline, preamble.start(), preamble.length());
  // Then jump back to the original.
  write_absolute_jump_64(trampoline + preamble.length(), request.original() + preamble.length());
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
