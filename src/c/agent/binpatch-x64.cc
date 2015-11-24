//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "binpatch-x64.hh"
#include "disass/disassembler-x86.hh"
#include "utils/types.hh"

using namespace conprx;

size_t X64::get_redirect_size_bytes() {
  return kRedirectSizeBytes;
}

void X64::install_redirect(PatchRequest &request) {
  address_t original = request.original();
  address_t replacement = request.replacement();

  // Write the redirect to the entry stub into the original.
  write_absolute_jump(original, replacement);

  // The redirect may partially overwrite the last instruction, leaving behind
  // invalid code. To avoid that we fill the last bit with one-byte
  // instructions (int3).
  if (kRedirectSizeBytes < request.preamble_size())
    memset(original + kRedirectSizeBytes, kInt3Op, request.preamble_size() - kRedirectSizeBytes);
}

void X64::write_absolute_jump(address_t code, address_t dest) {
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

bool X64::get_preamble_size_bytes(address_t addr, size_t *size_out) {
  // The length of this vector shouldn't matter since the disassembler reads
  // one byte at a time and stops as soon as we've seen enough. It doesn't
  // continue on to the end.
  Vector<byte_t> code(addr, kMaxPreambleSizeBytes);
  size_t offset = 0;
  Disassembler &disass = Disassembler::x86_64();
  while (offset < kRedirectSizeBytes) {
    InstructionInfo info;
    if (!disass.resolve(code, offset, &info)) {
      if (info.status() == InstructionInfo::BLACKLISTED) {
        WARN("Instruction 0x%x at offset %i is blacklisted",
            info.instruction(), offset);
      } else if (info.status() == InstructionInfo::INVALID_INSTRUCTION) {
        WARN("Invalid instruction 0x%x at offset %i was blacklisted",
            info.instruction(), offset);
      }
      // If the disassembler failed to resolve the instruction for whatever
      // reason, we bail out.
      return false;
    }
    offset += info.length();
  }
  *size_out = offset;
  return true;
}

void X64::write_trampoline(PatchRequest &request, PatchCode &code) {
  // Initially let the trampoline interrupt (int3) when called. Just in case
  // anyone should decide to call it in the case that we failed below.
  address_t trampoline = code.trampoline_;
  trampoline[0] = kInt3Op;
  // Copy the overwritten bytes into the trampoline. We'll definitely have to
  // execute those.
  Vector<byte_t> preamble = request.preamble();
  memcpy(trampoline, preamble.start(), preamble.length());
  // Then jump back to the original.
  write_absolute_jump(trampoline + preamble.length(), request.original() + preamble.length());
}

X64 &X64::get() {
  static X64 *instance = NULL;
  if (instance == NULL)
    instance = new X64();
  return *instance;
}
