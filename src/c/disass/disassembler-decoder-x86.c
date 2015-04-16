//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#define NDEBUG 1

#include "utils/types.hh"

// For off_t.
#include <sys/types.h>
#include <stdarg.h>

// We need this bit of setup for the headers to compile. Found by trial and
// error.
#define INSTRUCTION_SPECIFIER_FIELDS uint16_t operands;
#define INSTRUCTION_IDS uint16_t instructionIDs;
#define NDEBUG 1

// The BOOL defined below will clash with the one from windows.h if we're not
// careful.
#ifdef IS_MSVC
#define BOOL DONT_CLASH_WITH_WINDEF_BOOL
#endif

// Disable all these warnings for included files, there's nothing we can do to
// fix them anyway.
#ifdef IS_GCC
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wc++-compat"
#endif

#include "llvm/X86DisassemblerDecoderCommon.h"
#include "llvm/X86DisassemblerDecoder.h"
#include "llvm/X86GenDisassemblerTables.inc"
#include "llvm/X86DisassemblerDecoder.c"

#ifdef IS_GCC
#  pragma GCC diagnostic pop
#endif
