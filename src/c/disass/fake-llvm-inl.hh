//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "fake-llvm.hh"

namespace llvm {

template <typename K, typename V>
V &DenseMap<K, V>::operator[](const K &) {
  return value_;
}

template <typename T>
raw_ostream &raw_ostream::operator<<(const T& arg) {
  return *this;
}

template <typename T, size_t N>
T &SmallVector<T, N>::operator[](size_t idx) const {
  return *static_cast<T*>(NULL);
}

template <typename T, size_t N>
void SmallVector<T, N>::push_back(const T &arg) {
  // ignore
}

} // namespace llvm

// Disable debug output.
#define DEBUG(X)
