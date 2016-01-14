//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/binpatch-x86.hh"
#include "disass/disassembler-x86.hh"
#include "utils/types.hh"

using namespace conprx;

class X86_32 : public GenericX86 {
public:
  virtual void write_imposter(PatchRequest &request, tclib::Blob memory);
  virtual Disassembler *disassembler();
  virtual size_t optimal_preable_size() { return kJmpSize; }
  virtual Redirection *create_redirection(address_t original, address_t replacement,
      PreambleInfo *info, MessageSink *messages);

  // Returns the singleton ia32 instance.
  static X86_32 &get();

private:
  // Can you jump from the given address to the given destination with a
  // relative 32-bit wide jump instruction?
  bool can_jump_relative_32(address_t from, address_t to);
};

void X86_32::write_imposter(PatchRequest &request, tclib::Blob memory) {
  // Initially let the trampoline interrupt (int3) when called. Just in case
  // anyone should decide to call it in the case that we failed below.
  address_t trampoline = static_cast<address_t>(memory.start());
  trampoline[0] = kInt3;
  // Copy the overwritten bytes into the trampoline. We'll definitely have to
  // execute those.
  Vector<byte_t> preamble_copy = request.preamble_copy();
  memcpy(trampoline, preamble_copy.start(), preamble_copy.length());
  // Then jump back to the original.
  write_relative_jump_32(trampoline + preamble_copy.length(), request.original() + preamble_copy.length());
}

Redirection *X86_32::create_redirection(address_t original, address_t replacement,
    PreambleInfo *info, MessageSink *messages) {
  return new RelativeJump32Redirection();
}

bool X86_32::can_jump_relative_32(address_t from, address_t to) {
  ssize_t distance = from - to;
  return static_cast<int32_t>(distance) == distance;
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
