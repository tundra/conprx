//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/binpatch-x86.hh"
#include "c/valgrind.h"
#include "disass/disassembler-x86.hh"
#include "utils/types.hh"

using namespace conprx;

bool GenericX86::prepare_patch(address_t original, address_t replacement,
    address_t trampoline, size_t min_size_required, size_t *size_out,
    MessageSink *messages) {
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
        return REPORT_MESSAGE(messages, "Instruction 0x%02x at offset %i is not whitelisted",
            info.instruction(), offset);
      } else if (info.status() == InstructionInfo::INVALID_INSTRUCTION) {
        return REPORT_MESSAGE(messages, "Invalid instruction 0x%02x at offset %i was invalid",
            info.instruction(), offset);
      }
      // If the disassembler failed to resolve the instruction for whatever
      // reason, we bail out.
      return REPORT_MESSAGE(messages, "Disassembler failed to resolve byte 0x%02x at offset %i",
          code[offset], offset);
    }
    offset += info.length();
    if (info.status() == InstructionInfo::RESOLVED_END) {
      if (offset < min_size_required)
        return REPORT_MESSAGE(messages, "Not enough room to patch function; "
            "required %i found only %i (last instruction was 0x%02x)",
            min_size_required, offset, info.instruction());
      break;
    }
  }
  *size_out = offset;
  return true;
}

void GenericX86::flush_instruction_cache(tclib::Blob memory) {
  // There's no need to explicitly flush instruction caches on intel but
  // valgrind does need to be told.
  VALGRIND_DISCARD_TRANSLATIONS(memory.start(), memory.size());
}
