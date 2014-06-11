//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Fake implementations that stand in for the LLVM types in the disassembler
// code. This is to minimize the amount of LLVM code that needs to be copied
// into conprx.

#include "utils/types.hh"
#include "utils/vector.hh"

namespace conprx {

// No-behavior stand-in for DenseMap.
template <typename K, typename V>
class FakeDenseMap {
public:
  V &operator[](const K &);
private:
  V value_;
};

} // namespace conprx

// Define the llvm types in terms of the fakes.
namespace llvm {

class MCInst;
class MemoryObject;
class MCInstrInfo;

template <typename K, typename V>
class DenseMap : public conprx::FakeDenseMap<K, V> { };

template <typename T>
class isPodLike { };

struct SMLoc { };

typedef const char *StringRef;

// No-behavior subtarget info.
class MCSubtargetInfo {
public:
  uint64_t getFeatureBits() const;
};

// A raw ostream that ignores everything.
class raw_ostream {
public:
  template <typename T>
  raw_ostream &operator<<(const T &);

  // Returns the singleton instance.
  static raw_ostream &get() { return kInstance; }

private:
  static raw_ostream kInstance;
};

static raw_ostream &dbgs() {
  return raw_ostream::get();
}

static raw_ostream &nulls() {
  return raw_ostream::get();
}

// An llvm memory object that reads directly from a byte vector.
class MemoryObject {
public:
  MemoryObject(Vector<byte_t> memory);
  uint64_t getBase() const;
  uint64_t getExtent() const;
  int readByte(uint64_t address, uint8_t *ptr) const;
private:
  Vector<byte_t> memory_;
};

template <typename T>
class SmallVectorImpl {
public:
  typedef void *iterator;
};

// No-behavior SmallVector. Note that most of these methods have no
// implementations because for whatever reason the code compiles fine without
// them. Add them if it's ever a problem.
template <typename T, size_t N>
class SmallVector : public SmallVectorImpl<T> {
public:
  T &operator[] (size_t idx) const;
  size_t size() const;
  void push_back(const T &arg);
  void clear();
  void *begin();
  void *end();
  void *insert(void *i, const T &arg);
};

// The disassembler needs a minimum of functionality from its superclass.
class MCDisassembler {
public:
  enum DecodeStatus {
    Fail = 0,
    SoftFail = 1,
    Success = 3
  };

  virtual ~MCDisassembler();

  MCDisassembler(const MCSubtargetInfo&);

  virtual DecodeStatus  getInstruction(MCInst&, uint64_t&, const MemoryObject&,
      uint64_t, raw_ostream&, raw_ostream&) const = 0;

  virtual bool tryAddingSymbolicOperand(MCInst &, int64_t, uint64_t, bool,
      uint64_t, uint64_t) const;

  virtual void tryAddingPcLoadReferenceComment(int64_t, uint64_t) const;

  mutable raw_ostream *CommentStream;
};

// A null target.
class Target {
public:
  MCInstrInfo *createMCInstrInfo() const;
};

extern Target TheX86_32Target;
extern Target TheX86_64Target;

// Do-nothing target registry.
class TargetRegistry {
public:
  static void RegisterMCDisassembler(const Target&,
      MCDisassembler *(*)(const Target&, const MCSubtargetInfo&));
};

} // namespace llvm
