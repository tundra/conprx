//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _DISASSEMBLER_X86
#define _DISASSEMBLER_X86

#include "utils/vector.hh"

namespace conprx {

// Wrapper around an llvm disassembler.
class Disassembler {
public:
  virtual ~Disassembler();
  virtual size_t get_instruction_length(Vector<byte_t> code) = 0;
  static Disassembler &x86_64();
};

} // namespace conprx

#endif // _DISASSEMBLER_X86
