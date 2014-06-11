#!/bin/sh

set -e

# This script grabs the files we need for the disassembler from the LLVM source
# distribution.

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <llvm root> <conprx root>" >&2
  exit 1
fi

LLVM_VERSION=3.4
LLVM_ROOT=$1
CONPRX_ROOT=$2

TARGET_ROOT=$CONPRX_ROOT/src/c/disass/llvm
LLVM_TBLGEN=llvm-tblgen-$LLVM_VERSION

# tblgen --gen-register-info
REGISTER_INFO=$TARGET_ROOT/X86GenRegisterInfo.inc
if [ ! -f $REGISTER_INFO ]; then
  cd $LLVM_ROOT/lib/Target/X86
  $LLVM_TBLGEN --gen-register-info -I=$LLVM_ROOT/include X86.td > $REGISTER_INFO
fi

# tblgen --gen-instr-info
INSTR_INFO=$TARGET_ROOT/X86GenInstrInfo.inc
if [ ! -f $INSTR_INFO ]; then
  cd $LLVM_ROOT/lib/Target/X86
  $LLVM_TBLGEN --gen-instr-info -I=$LLVM_ROOT/include X86.td > $INSTR_INFO
fi

# tblgen --gen-disassembler
TABLES=$TARGET_ROOT/X86GenDisassemblerTables.inc
if [ ! -f $TABLES ]; then
  cd $LLVM_ROOT/lib/Target/X86
  $LLVM_TBLGEN --gen-disassembler -I=$LLVM_ROOT/include X86.td > $TABLES
fi

# Copy, stripping includes. 
cpsi() {
  cat $1 | sed -e 's|\(#include\)|// \1|g' > $2
}

# Copy the disassembler source files.
DISASS_SRC_ROOT=$LLVM_ROOT/lib/Target/X86/Disassembler/
cpsi $DISASS_SRC_ROOT/X86Disassembler.cpp $TARGET_ROOT/X86Disassembler.cc
cpsi $DISASS_SRC_ROOT/X86DisassemblerDecoder.c $TARGET_ROOT/X86DisassemblerDecoder.c
cpsi $DISASS_SRC_ROOT/X86Disassembler.h $TARGET_ROOT/X86Disassembler.h
cpsi $DISASS_SRC_ROOT/X86DisassemblerDecoderCommon.h $TARGET_ROOT/X86DisassemblerDecoderCommon.h
cpsi $DISASS_SRC_ROOT/X86DisassemblerDecoder.h $TARGET_ROOT/X86DisassemblerDecoder.h

# Copy the support includes.
INCLUDE_SRC_ROOT=$LLVM_ROOT/include
cpsi $INCLUDE_SRC_ROOT/llvm/MC/MCInst.h $TARGET_ROOT/MCInst.hh
cpsi $INCLUDE_SRC_ROOT/llvm/MC/MCRegisterInfo.h $TARGET_ROOT/MCRegisterInfo.hh
cpsi $INCLUDE_SRC_ROOT/llvm/MC/MCInstrInfo.h $TARGET_ROOT/MCInstrInfo.hh
cpsi $INCLUDE_SRC_ROOT/llvm/MC/MCInstrDesc.h $TARGET_ROOT/MCInstrDesc.hh
