//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/binpatch-x86.hh"
#include "disass/disassembler-x86.hh"
#include "utils/types.hh"

using namespace conprx;

class X86_32 : public GenericX86 {
public:
  virtual size_t redirect_size_bytes();
  virtual void install_redirect(PatchRequest &request);
  virtual void write_trampoline(PatchRequest &request, PatchCode &code);
  virtual Disassembler *disassembler();
  virtual bool validate_code_locations(address_t original, address_t replacement,
      address_t trampoline, MessageSink *messages);

  // Returns the singleton ia32 instance.
  static X86_32 &get();

private:
  // Can you jump from the given address to the given destination with a
  // relative 32-bit wide jump instruction?
  bool can_jump_relative_32(address_t from, address_t to);

  // Write a relative 32-bit jump at the given location to the given
  // destination.
  static void write_relative_jump_32(address_t code, address_t dest);

  static const byte_t kJmp = 0xe9;
  static const size_t kJmpSize = 5;
  static const size_t kRedirectSizeBytes = kJmpSize;
};

size_t X86_32::redirect_size_bytes() {
  return kRedirectSizeBytes;
}

bool X86_32::validate_code_locations(address_t original, address_t replacement,
      address_t trampoline, MessageSink *messages) {
  if (!can_jump_relative_32(original, replacement)) {
    return REPORT_MESSAGE(messages, "Can't jump from original %p to %p in 32 bits",
        original, replacement);
  } else if (!can_jump_relative_32(trampoline, original)) {
    return REPORT_MESSAGE(messages, "Can't jump from trampoline %p to %p in 32 bits",
        trampoline, original);
  }
  return true;
}

void X86_32::install_redirect(PatchRequest &request) {
  address_t original = request.original();
  address_t replacement = request.replacement();

  // Write the redirect to the entry stub into the original.
  write_relative_jump_32(original, replacement);

  // The redirect may partially overwrite the last instruction, leaving behind
  // invalid code. To avoid that we fill the last bit with one-byte
  // instructions (int3).
  if (kRedirectSizeBytes < request.preamble_size())
    memset(original + kRedirectSizeBytes, kInt3, request.preamble_size() - kRedirectSizeBytes);
}

void X86_32::write_trampoline(PatchRequest &request, PatchCode &code) {
  // Initially let the trampoline interrupt (int3) when called. Just in case
  // anyone should decide to call it in the case that we failed below.
  address_t trampoline = code.trampoline_;
  trampoline[0] = kInt3;
  // Copy the overwritten bytes into the trampoline. We'll definitely have to
  // execute those.
  Vector<byte_t> preamble = request.preamble();
  memcpy(trampoline, preamble.start(), preamble.length());
  // Then jump back to the original.
  write_relative_jump_32(trampoline + preamble.length(), request.original() + preamble.length());
}

bool X86_32::can_jump_relative_32(address_t from, address_t to) {
  ssize_t distance = from - to;
  return static_cast<int32_t>(distance) == distance;
}
// Debug: Redirecting 0x80ae365 to 0x80ae36e
void X86_32::write_relative_jump_32(address_t code, address_t dest) {
  int32_t distance = static_cast<int32_t>(dest - (code + kJmpSize));
  code[0] = kJmp;
  *reinterpret_cast<int32_t*>(code + 1) = distance;
}

Disassembler *X86_32::disassembler() {
  return &Disassembler::x86_32();
}

X86_32 &X86_32::get() {
  static X86_32 *instance = NULL;
  if (instance == NULL)
    instance = new X86_32();
  return *instance;
}

InstructionSet &InstructionSet::get() {
  return X86_32::get();
}

#include "binpatch-x86.cc"
