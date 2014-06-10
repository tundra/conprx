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
  byte_t block[8];
  memcpy(block, request.overwritten().start(), kRedirectSizeBytes);
  memcpy(block + kRedirectSizeBytes, request.original() + kRedirectSizeBytes, 8 - kRedirectSizeBytes);
  size_t offset = 0;
  while (offset < kRedirectSizeBytes) {
    size_t next_size = get_instruction_length(Vector<byte_t>(block + offset, 8 - offset));
    if (next_size == 0)
      return false;
    offset += next_size;
  }
  return true;
}

size_t X64::get_instruction_length(Vector<byte_t> code) {
  return Disassembler::x86_64().get_instruction_length(code);
}

X64 &X64::get() {
  static X64 *instance = NULL;
  if (instance == NULL)
    instance = new X64();
  return *instance;
}
