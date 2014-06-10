#!/bin/sh

set -e

# This script grabs the files we need for the disassembler from the LLVM source
# distribution.

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <llvm root> <conprx root>" >&2
  exit 1
fi

LLVM_ROOT=$1
DISASS_ROOT=$2/src/c/disass
TABLES=$DISASS_ROOT/X86GenDisassemblerTables.inc
REGISTER_INFO=$DISASS_ROOT/X86GenRegisterInfo.inc
INSTR_INFO=$DISASS_ROOT/X86GenInstrInfo.inc

if [ ! -f $REGISTER_INFO ]; then
  cd $LLVM_ROOT/lib/Target/X86
  llvm-tblgen-3.4 --gen-register-info -I=$LLVM_ROOT/include X86.td > $REGISTER_INFO
fi

if [ ! -f $INSTR_INFO ]; then
  cd $LLVM_ROOT/lib/Target/X86
  llvm-tblgen-3.4 --gen-instr-info -I=$LLVM_ROOT/include X86.td > $INSTR_INFO
fi

if [ ! -f $TABLES ]; then
  cd $LLVM_ROOT/lib/Target/X86
  llvm-tblgen-3.4 --gen-disassembler -I=$LLVM_ROOT/include X86.td > $TABLES
fi

SOURCE_ROOT=$LLVM_ROOT/lib/Target/X86/Disassembler/
cp $SOURCE_ROOT/X86Disassembler.cpp $DISASS_ROOT/X86Disassembler.cc
cp $SOURCE_ROOT/X86DisassemblerDecoder.c $DISASS_ROOT/X86DisassemblerDecoder.c

cp $SOURCE_ROOT/X86Disassembler.h $DISASS_ROOT/X86Disassembler.h
cp $SOURCE_ROOT/X86DisassemblerDecoderCommon.h $DISASS_ROOT/X86DisassemblerDecoderCommon.h
cp $SOURCE_ROOT/X86DisassemblerDecoder.h $DISASS_ROOT/X86DisassemblerDecoder.h
