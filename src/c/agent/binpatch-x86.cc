//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/binpatch-x86.hh"
#include "c/valgrind.h"
#include "disass/disassembler-x86.hh"
#include "utils/types.hh"

using namespace conprx;
using namespace tclib;

fat_bool_t GenericX86::prepare_patch(address_t original, address_t replacement,
      pass_def_ref_t<Redirection> *redir_out, PreambleInfo *info_out) {
  // The length of this vector shouldn't matter since the disassembler reads
  // one byte at a time and stops as soon as we've seen enough. It doesn't
  // continue on to the end.
  Vector<byte_t> code(original, kMaxPreambleSizeBytes);
  size_t offset = 0;
  // This is the amount of space we're hoping to allocate, though we'll be okay
  // if we just get more than min_size.
  Disassembler *disass = disassembler();
  byte_t last_instr = 0;
  size_t target_size = optimal_preable_size();
  while (offset < target_size) {
    InstructionInfo info;
    if (!disass->resolve(code, offset, &info)) {
      // Failed to even resolve the instruction. Report an error depending on
      // the cause.
      if (info.status() == InstructionInfo::UNKNOWN) {
        LOG_WARN("Instruction 0x%02x at offset %i is unknown", info.instruction(),
            offset);
        return F_FALSE;
      } else if (info.status() == InstructionInfo::INVALID) {
        LOG_WARN("Instruction at offset %i was invalid", offset);
        return F_FALSE;
      } else {
        LOG_WARN("Disassembler failed to resolve byte 0x%02x at offset %i",
          code[offset], offset);
        return F_FALSE;
      }
    }
    last_instr = info.instruction();
    if (info.status() != InstructionInfo::SENSITIVE)
      // We successfully decoded the instruction and it's one we're allowed to
      // skip over, so we skip over it.
      offset += info.length();
    if (info.status() != InstructionInfo::BENIGN)
      // Whether we skipped or not, we've gotten as far as we're allowed to get.
      break;
  }
  info_out->populate(offset, last_instr);
  return create_redirection(original, replacement, redir_out, info_out);
}

void GenericX86::flush_instruction_cache(tclib::Blob memory) {
  // There's no need to explicitly flush instruction caches on intel but
  // valgrind does need to be told.
  VALGRIND_DISCARD_TRANSLATIONS(memory.start(), memory.size());
}

void GenericX86::write_halt(tclib::Blob memory) {
  blob_fill(memory, kInt3);
}

size_t GenericX86::write_relative_jump_32(address_t code, address_t dest) {
  int32_t distance = static_cast<int32_t>(dest - (code + kJmpSize));
  code[0] = kJmp;
  *reinterpret_cast<int32_t*>(code + 1) = distance;
  return kJmpSize;
}

bool GenericX86::can_jump_relative_32(address_t from, address_t to) {
  ONLY_32_BIT(return true);
  ssize_t distance = (from + kJmpSize) - to;
  return static_cast<int32_t>(distance) == distance;
}

size_t GenericX86::RelativeJump32Redirection::write_redirect(address_t code, address_t dest) {
  return write_relative_jump_32(code, dest);
}
