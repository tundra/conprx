//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Vector utility.

#include "utils/vector.hh"
#include "test.hh"

TEST(vector, simple) {
  Vector<void*> v0;
  ASSERT_EQ(0, v0.length());
  int32_t elms[3] = {7, 3, 11};
  Vector<int32_t> v3(elms, 3);
  ASSERT_EQ(3, v3.length());
  ASSERT_EQ(7, v3[0]);
  ASSERT_EQ(3, v3[1]);
  ASSERT_EQ(11, v3[2]);
}

TEST(vector, distance) {
  int32_t ints[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  ASSERT_EQ(10, Vector<int32_t>(ints, 10).distance(Vector<int32_t>(ints, 10)));
  ASSERT_EQ(10, Vector<int32_t>(ints, 10).distance(Vector<int32_t>(ints, 5)));
  ASSERT_EQ(10, Vector<int32_t>(ints, 5).distance(Vector<int32_t>(ints, 10)));
  ASSERT_EQ(10, Vector<int32_t>(ints, 2).distance(Vector<int32_t>(ints + 8, 2)));
  ASSERT_EQ(8, Vector<int32_t>(ints + 1, 2).distance(Vector<int32_t>(ints + 7, 2)));
  ASSERT_EQ(6, Vector<int32_t>(ints, 10).distance(Vector<int32_t>(ints + 4, 2)));
  ASSERT_EQ(4, Vector<int32_t>(ints, 2).distance(Vector<int32_t>(ints + 2, 2)));
  int8_t bytes[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  ASSERT_EQ(8, Vector<int8_t>(bytes + 1, 2).distance(Vector<int8_t>(bytes + 7, 2)));
}

TEST(vector, cast) {
  int32_t ints[3] = {6, 5, 4};
  Vector<int8_t> v8 = Vector<int32_t>(ints, 3).cast<int8_t>();
  ASSERT_EQ(3 * sizeof(int32_t), v8.length());
  Vector<int32_t> v32 = v8.cast<int32_t>();
  ASSERT_EQ(3, v32.length());
  ASSERT_EQ(6, v32[0]);
  ASSERT_EQ(5, v32[1]);
  ASSERT_EQ(4, v32[2]);
}

TEST(vector, has_suffix) {
  int32_t ints[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  ASSERT_TRUE(Vector<int32_t>(ints, 10).has_suffix(Vector<int32_t>(ints, 10)));
  ASSERT_TRUE(Vector<int32_t>(ints, 10).has_suffix(Vector<int32_t>(ints + 1, 9)));
  ASSERT_TRUE(Vector<int32_t>(ints, 10).has_suffix(Vector<int32_t>(ints + 5, 5)));
  ASSERT_TRUE(Vector<int32_t>(ints, 10).has_suffix(Vector<int32_t>(ints + 9, 1)));
  ASSERT_FALSE(Vector<int32_t>(ints + 1, 9).has_suffix(Vector<int32_t>(ints, 10)));
  ASSERT_TRUE(Vector<int32_t>(ints + 1, 9).has_suffix(Vector<int32_t>(ints + 2, 8)));
  ASSERT_TRUE(Vector<int32_t>(ints, 10).has_suffix(Vector<int32_t>()));
  int32_t oints[10] = {10, 11, 12, 3, 4, 5, 6, 7, 8, 9};
  ASSERT_FALSE(Vector<int32_t>(ints, 10).has_suffix(Vector<int32_t>(oints, 10)));
  ASSERT_FALSE(Vector<int32_t>(ints, 10).has_suffix(Vector<int32_t>(oints + 1, 9)));
  ASSERT_FALSE(Vector<int32_t>(ints, 10).has_suffix(Vector<int32_t>(oints + 2, 8)));
  ASSERT_TRUE(Vector<int32_t>(ints, 10).has_suffix(Vector<int32_t>(oints + 3, 7)));
}
