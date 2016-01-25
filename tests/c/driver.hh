//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "c/stdc.h"

#include "plankton-inl.hh"

typedef enum {
  BEGIN,
  END
} opcode_t;

class Instruction {
public:
  Instruction(opcode_t opcode, plankton::Variant args)
    : opcode_(opcode)
    , args_(args) { }
private:
  opcode_t opcode_;
  plankton::Variant args_;
};
