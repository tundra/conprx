#!/bin/sh

set -e

# This script grabs the files we need for the disassembler from the LLVM source
# distribution.

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <llvm root> <conprx root>" >&2
  exit 1
fi

LLVM_ROOT=$1
CONPRX_ROOT=$2
DISASS_TRG_ROOT=$CONPRX_ROOT/src/c/disass
TABLES=$DISASS_TRG_ROOT/X86GenDisassemblerTables.inc
REGISTER_INFO=$DISASS_TRG_ROOT/X86GenRegisterInfo.inc
INSTR_INFO=$DISASS_TRG_ROOT/X86GenInstrInfo.inc

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

strcp() {
  cat $1 | grep -v -e '#include "llvm' > $2
}

DISASS_SRC_ROOT=$LLVM_ROOT/lib/Target/X86/Disassembler/
strcp $DISASS_SRC_ROOT/X86Disassembler.cpp $DISASS_TRG_ROOT/X86Disassembler.cc
strcp $DISASS_SRC_ROOT/X86DisassemblerDecoder.c $DISASS_TRG_ROOT/X86DisassemblerDecoder.c

strcp $DISASS_SRC_ROOT/X86Disassembler.h $DISASS_TRG_ROOT/X86Disassembler.h
strcp $DISASS_SRC_ROOT/X86DisassemblerDecoderCommon.h $DISASS_TRG_ROOT/X86DisassemblerDecoderCommon.h
strcp $DISASS_SRC_ROOT/X86DisassemblerDecoder.h $DISASS_TRG_ROOT/X86DisassemblerDecoder.h

INCLUDE_SRC_ROOT=$LLVM_ROOT/include
INCLUDE_TRG_ROOT=$CONPRX_ROOT/src/c/disass
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCInstrItineraries.h $INCLUDE_TRG_ROOT/MCInstrItineraries.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCSchedule.h $INCLUDE_TRG_ROOT/MCSchedule.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCInst.h $INCLUDE_TRG_ROOT/MCInst.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCSubtargetInfo.h $INCLUDE_TRG_ROOT/MCSubtargetInfo.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCInstrInfo.h $INCLUDE_TRG_ROOT/MCInstrInfo.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCInstrDesc.h $INCLUDE_TRG_ROOT/MCInstrDesc.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCRegisterInfo.h $INCLUDE_TRG_ROOT/MCRegisterInfo.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCDisassembler.h $INCLUDE_TRG_ROOT/MCDisassembler.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCSymbolizer.h $INCLUDE_TRG_ROOT/MCSymbolizer.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/MCRelocationInfo.h $INCLUDE_TRG_ROOT/MCRelocationInfo.hh
strcp $INCLUDE_SRC_ROOT/llvm/MC/SubtargetFeature.h $INCLUDE_TRG_ROOT/SubtargetFeature.hh
strcp $INCLUDE_SRC_ROOT/llvm-c/Disassembler.h $INCLUDE_TRG_ROOT/Disassembler.hh
strcp $INCLUDE_SRC_ROOT/llvm-c/TargetMachine.h $INCLUDE_TRG_ROOT/TargetMachine.hh
strcp $INCLUDE_SRC_ROOT/llvm-c/Target.h $INCLUDE_TRG_ROOT/Target.hh
strcp $INCLUDE_SRC_ROOT/llvm-c/Core.h $INCLUDE_TRG_ROOT/Core.hh

strcp $INCLUDE_SRC_ROOT/llvm/Support/PointerLikeTypeTraits.h $INCLUDE_TRG_ROOT/PointerLikeTypeTraits.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/ErrorHandling.h $INCLUDE_TRG_ROOT/ErrorHandling.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/Compiler.h $INCLUDE_TRG_ROOT/Compiler.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/AlignOf.h $INCLUDE_TRG_ROOT/AlignOf.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/MathExtras.h $INCLUDE_TRG_ROOT/MathExtras.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/type_traits.h $INCLUDE_TRG_ROOT/type_traits.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/raw_ostream.h $INCLUDE_TRG_ROOT/raw_ostream.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/SwapByteOrder.h $INCLUDE_TRG_ROOT/SwapByteOrder.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/SMLoc.h $INCLUDE_TRG_ROOT/SMLoc.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/Debug.h $INCLUDE_TRG_ROOT/Debug.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/FileSystem.h $INCLUDE_TRG_ROOT/FileSystem.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/TimeValue.h $INCLUDE_TRG_ROOT/TimeValue.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/Casting.h $INCLUDE_TRG_ROOT/Casting.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/system_error.h $INCLUDE_TRG_ROOT/system_error.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/MemoryObject.h $INCLUDE_TRG_ROOT/MemoryObject.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/TargetRegistry.h $INCLUDE_TRG_ROOT/TargetRegistry.hh
strcp $INCLUDE_SRC_ROOT/llvm/Support/CodeGen.h $INCLUDE_TRG_ROOT/CodeGen.hh

strcp $INCLUDE_SRC_ROOT/llvm/ADT/OwningPtr.h $INCLUDE_TRG_ROOT/OwningPtr.hh
strcp $INCLUDE_SRC_ROOT/llvm/ADT/DenseMap.h $INCLUDE_TRG_ROOT/DenseMap.hh
strcp $INCLUDE_SRC_ROOT/llvm/ADT/DenseMapInfo.h $INCLUDE_TRG_ROOT/DenseMapInfo.hh
strcp $INCLUDE_SRC_ROOT/llvm/ADT/Triple.h $INCLUDE_TRG_ROOT/Triple.hh
strcp $INCLUDE_SRC_ROOT/llvm/ADT/Twine.h $INCLUDE_TRG_ROOT/Twine.hh
strcp $INCLUDE_SRC_ROOT/llvm/ADT/SmallVector.h $INCLUDE_TRG_ROOT/SmallVector.hh
strcp $INCLUDE_SRC_ROOT/llvm/ADT/StringRef.h $INCLUDE_TRG_ROOT/StringRef.hh
strcp $INCLUDE_SRC_ROOT/llvm/ADT/SmallString.h $INCLUDE_TRG_ROOT/SmallString.hh
strcp $INCLUDE_SRC_ROOT/llvm/ADT/IntrusiveRefCntPtr.h $INCLUDE_TRG_ROOT/IntrusiveRefCntPtr.hh
