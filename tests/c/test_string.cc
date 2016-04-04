//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"

#include "utils/string.hh"

using namespace conprx;

TEST(string, msdos_codec) {
  for (size_t i = 0; i < 256; i++) {
    uint8_t ac = static_cast<uint8_t>(i);
    uint16_t wc = MsDosCodec::ansi_to_wide_char(ac);
    uint8_t aca = MsDosCodec::wide_to_ansi_char(wc);
    ASSERT_EQ(ac, aca);
    if (i < 128)
      ASSERT_EQ(ac, wc);
  }
  bool ansi_seen[256];
  struct_zero_fill(ansi_seen);
  for (size_t i = 0; i < 65536; i++) {
    uint16_t wc = static_cast<uint16_t>(i);
    uint8_t ac = MsDosCodec::wide_to_ansi_char(wc);
    if (ac != '?') {
      ASSERT_FALSE(ansi_seen[ac]);
      ansi_seen[ac] = true;
    }
  }
}
