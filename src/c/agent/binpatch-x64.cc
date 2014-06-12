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
}



bool X64::get_preamble_length(PatchRequest &request, size_t *length_out) {
  // TODO: figure out if this is safe or if it might lead to reading past the
  //   end of the page that holds the original function.
#define kSnippetSize 16
  // Build an array of bytes containing the original code which is now split
  // in two.
  byte_t bytes[kSnippetSize];
  memcpy(bytes, request.overwritten().start(), kRedirectSizeBytes);
  memcpy(bytes + kRedirectSizeBytes, request.original() + kRedirectSizeBytes,
      kSnippetSize - kRedirectSizeBytes);
  Vector<byte_t> block(bytes, kSnippetSize);
  // Skip over one instruction at a time until we're past the redirect.
  size_t offset = 0;
  Disassembler &disass = Disassembler::x86_64();
  while (offset < kRedirectSizeBytes) {
    InstructionInfo info;
    if (!disass.resolve(block, offset, &info)) {
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
  *length_out = offset;
  return true;
}

bool X64::write_trampoline(PatchRequest &request, PatchCode &code) {
  // Initially let the trampoline interrupt (int3) when called. Just in case
  // anyone should decide to call it in the case that we failed below.
  address_t trampoline = code.trampoline_;
  trampoline[0] = kInt3Op;
  // Determine the amount of code to restore.
  size_t preamble_length = 0;
  if (!get_preamble_length(request, &preamble_length))
    return false;
  // Copy the overwritten bytes into the trampoline. We'll definitely have to
  // execute those.
  memcpy(trampoline, request.overwritten().start(), kRedirectSizeBytes);
  // Then, if there are more bytes to copy, do that as well.
  size_t remaining_length = preamble_length - kRedirectSizeBytes;
  if (remaining_length > 0) {
    memcpy(trampoline + kRedirectSizeBytes, request.original() + kRedirectSizeBytes,
        remaining_length);
  }
  // Now we can jump back to the original. Note that we don't have to account
  // for the preamble length because even thought we're jumping from that far
  // into the trampoline the destination is the same length within the original
  // so they cancel out.
  ssize_t trampoline_to_original = (request.original() - trampoline) - kJmpOpSizeBytes;
  address_t jmp_addr = trampoline + preamble_length;
  jmp_addr[0] = kJmpOp;
  reinterpret_cast<int32_t*>(jmp_addr + 1)[0] = static_cast<int32_t>(trampoline_to_original);
  return true;
}

X64 &X64::get() {
  static X64 *instance = NULL;
  if (instance == NULL)
    instance = new X64();
  return *instance;
}
