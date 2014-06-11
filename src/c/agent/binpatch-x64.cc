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

bool X64::write_trampoline(PatchRequest &request, PatchCode &code) {
  address_t trampoline = code.trampoline_;
  trampoline[0] = kInt3Op;
  byte_t bytes[8];
  memcpy(bytes, request.overwritten().start(), kRedirectSizeBytes);
  memcpy(bytes + kRedirectSizeBytes, request.original() + kRedirectSizeBytes, 8 - kRedirectSizeBytes);
  Vector<byte_t> block(bytes, 8);
  size_t offset = 0;
  Disassembler::resolve_result result;
  Disassembler &disass = Disassembler::x86_64();
  while (offset < kRedirectSizeBytes) {
    if (disass.resolve(block, offset, &result) != Disassembler::RESOLVED)
      return false;
    offset += result.length;
  }
  return true;
}

X64 &X64::get() {
  static X64 *instance = NULL;
  if (instance == NULL)
    instance = new X64();
  return *instance;
}
