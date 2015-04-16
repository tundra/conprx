//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _VECTOR
#define _VECTOR

#include "c/stdc.h"

// A thin wrapper around an array with a length and various utilities.
template <typename T>
class Vector {
public:
  // Initializes an empty vector.
  Vector() : elms_(NULL), length_(0) { }

  // Initializes a vector with the given elements and length.
  Vector(T *elms, size_t length) : elms_(elms), length_(length) { }

  // Initializes a vector that covers the given range of addresses.
  Vector(T *start, T *end) : elms_(start), length_(end - start) { }

  // Returns true if this vector has no elements.
  bool is_empty() { return length() == 0; }

  // Returns the number of elements in this vector.
  size_t length() const { return length_; }

  // Returns the address of the beginning of this vector.
  T *start() { return elms_; }
  const T *start() const { return elms_; }

  // Returns the address of the end of this vector, that is, just past the end
  // of the last element.
  T *end() { return elms_ + length_; }
  const T *end() const { return elms_ + length_; }

  // Returns a vector that holds the same memory as this one but where the
  // elements are viewed as type S rather than T. The length is also adjusted
  // appropriately.
  template <typename S>
  Vector<S> cast() {
    return Vector<S>(reinterpret_cast<S*>(elms_), (length_ * sizeof(T)) / sizeof(S));
  }

  // Returns the index'th element.
  T &operator[](size_t index) { return elms_[index]; }

  // Returns the index'th element of a const vector.
  const T &operator[](size_t index) const { return elms_[index]; }

  // Returns true if the given vector is a suffix of this one. A vector is a
  // suffix of itself; the empty vector is trivially a suffix of any vector,
  // also including itself.
  bool has_suffix(Vector<T> other) {
    if (other.length() > length())
      return false;
    for (size_t i = 0; i < other.length(); i++) {
      T &t = (*this)[length() - i - 1];
      T &o = other[other.length() - i - 1];
      if (t != o)
        return false;
    }
    return true;
  }

  // Returns the distance between this vector and the given other vector. The
  // distance is measured as the largest number of elements between two
  // elements, one in this vector and one in the other. Hence the distance
  // between a vector and itself it its length.
  size_t distance(const Vector<T> &that) {
    size_t result =                 delta(start(), that.start());
           result = maximum(result, delta(start(), that.end()));
           result = maximum(result, delta(end(), that.start()));
           result = maximum(result, delta(end(), that.end()));
    return result;
  }

private:
  // Given two pointers, returns the distance between them.
  template <typename E>
  static size_t delta(const E *a, const E *b) {
    return (a < b) ? (b - a) : (a - b);
  }

  // Returns the greatest of two sizes.
  static size_t maximum(size_t a, size_t b) {
    return (a > b) ? a : b;
  }

  T *elms_;
  size_t length_;
};

#endif // _VECTOR
