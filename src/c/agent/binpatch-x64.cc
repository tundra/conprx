//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "binpatch-x64.hh"
#include "disass/disassembler-x86.hh"

using namespace conprx;

size_t X64::get_redirect_size_bytes() {
  return kRedirectSizeBytes;
}

void X64::install_redirect(PatchRequest &request) {
  address_t original = request.original();
  address_t replacement = request.replacement();

  // Write the redirect to the entry stub into the original.
  ssize_t original_to_replacement = (replacement - original) - kJmpOpSizeBytes;
  original[0] = kJmpOp;
  reinterpret_cast<int32_t*>(original + 1)[0] = static_cast<int32_t>(original_to_replacement);

  // The redirect may partially overwrite the last instruction, leaving behind
  // invalid code. To avoid that we fill the last bit with one-byte
  // instructions (int3).
  if (kJmpOpSizeBytes < request.preamble_size())
    memset(original + kJmpOpSizeBytes, kInt3Op, request.preamble_size() - kJmpOpSizeBytes);
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
        LOG_WARNING("Instruction 0x%x at offset %i is blacklisted",
            info.instruction(), offset);
      } else if (info.status() == InstructionInfo::INVALID_INSTRUCTION) {
        LOG_WARNING("Invalid instruction 0x%x at offset %i was blacklisted",
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
  // Now we can jump back to the original. Note that we don't have to account
  // for the preamble length because even thought we're jumping from that far
  // into the trampoline the destination is the same length within the original
  // so they cancel out.
  ssize_t trampoline_to_original = (request.original() - trampoline) - kJmpOpSizeBytes;
  address_t jmp_addr = trampoline + preamble.length();
  jmp_addr[0] = kJmpOp;
  reinterpret_cast<int32_t*>(jmp_addr + 1)[0] = static_cast<int32_t>(trampoline_to_original);
}

X64 &X64::get() {
  static X64 *instance = NULL;
  if (instance == NULL)
    instance = new X64();
  return *instance;
}
