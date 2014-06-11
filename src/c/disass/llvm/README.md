X86 disassembler code from LLVM
===============================

This code is taken from LLVM and adapted for use as a part of conprx. It is all
covered by the LLVM license in the LICENSE file.

All files in this directory except for this one, LICENSE, and import.sh have
been produced by running import.sh which generates the .inc files using tblgen
and copies the others from the LLVM source distribution, commenting out all
imports in the process. Otherwise the files are unchanged.
