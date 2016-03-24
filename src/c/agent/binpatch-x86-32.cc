//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/binpatch-x86.hh"
#include "disass/disassembler-x86.hh"
#include "utils/types.hh"

using namespace conprx;
using namespace tclib;

class X86_32 : public GenericX86 {
public:
  virtual size_t write_imposter(PatchRequest &request, tclib::Blob memory);
  virtual Disassembler *disassembler();
  virtual size_t optimal_preable_size() { return kJmpSize; }
  virtual fat_bool_t create_redirection(PatchRequest *request, ProximityAllocator *alloc,
      pass_def_ref_t<Redirection> *redir_out, PreambleInfo *info);

  // Returns the singleton ia32 instance.
  static X86_32 &get();

private:
  // Can you jump from the given address to the given destination with a
  // relative 32-bit wide jump instruction?
  bool can_jump_relative_32(address_t from, address_t to);
};

size_t X86_32::write_imposter(PatchRequest &request, tclib::Blob memory) {
  address_t trampoline = static_cast<address_t>(memory.start());
  // Copy the overwritten bytes into the trampoline. We'll definitely have to
  // execute those.
  tclib::Blob preamble_copy = request.preamble_copy();
  memcpy(trampoline, preamble_copy.start(), preamble_copy.size());
  // Then jump back to the original.
  return write_relative_jump_32(trampoline + preamble_copy.size(),
      request.original() + preamble_copy.size());
}

fat_bool_t X86_32::create_redirection(PatchRequest *request, ProximityAllocator *alloc,
    pass_def_ref_t<Redirection> *redir_out, PreambleInfo *info) {
  *redir_out = new (kDefaultAlloc) RelativeJump32Redirection();
  return F_TRUE;
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
