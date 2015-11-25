//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/binpatch-x86.hh"
#include "disass/disassembler-x86.hh"
#include "utils/types.hh"

using namespace conprx;

bool GenericX86::prepare_patch(address_t original, address_t replacement,
    address_t trampoline, size_t *size_out, MessageSink *messages) {
  // The length of this vector shouldn't matter since the disassembler reads
  // one byte at a time and stops as soon as we've seen enough. It doesn't
  // continue on to the end.
  if (!validate_code_locations(original, replacement, trampoline, messages))
    return false;
  Vector<byte_t> code(original, kMaxPreambleSizeBytes);
  size_t offset = 0;
  size_t limit = redirect_size_bytes();
  Disassembler *disass = disassembler();
  while (offset < limit) {
    InstructionInfo info;
    if (!disass->resolve(code, offset, &info)) {
      if (info.status() == InstructionInfo::NOT_WHITELISTED) {
        return messages->report("Instruction 0x%x at offset %i is not whitelisted",
            info.instruction(), offset);
      } else if (info.status() == InstructionInfo::INVALID_INSTRUCTION) {
        return messages->report("Invalid instruction 0x%x at offset %i was invalid",
            info.instruction(), offset);
      }
      // If the disassembler failed to resolve the instruction for whatever
      // reason, we bail out.
      return messages->report("Disassembler failed to resolve byte 0x%x at offset %i",
          code[offset], offset);
    }
    offset += info.length();
  }
  *size_out = offset;
  return true;
}
