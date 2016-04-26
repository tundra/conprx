//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "rpc.hh"
#include "test.hh"
#include "utils/string.hh"
#include "conback-utils.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;
using namespace conprx;

TEST(conback, simulated) {
  BasicConsoleBackend backend;
  SimulatedFrontendAdaptor frontend(&backend);
  ASSERT_TRUE(frontend.initialize());
  ASSERT_EQ(0, frontend->poke_backend(423));
  ASSERT_EQ(423, frontend->poke_backend(653));
  ASSERT_EQ(653, frontend->poke_backend(374));
  ASSERT_EQ(374, backend.last_poke());
}

CONBACK_TEST(conback, cp) {
  CONBACK_TEST_PREAMBLE();

  ASSERT_TRUE(frontend->set_console_cp(cpUtf8));
  ASSERT_TRUE(frontend->set_console_output_cp(cpUtf8));
  ASSERT_EQ(cpUtf8, frontend->get_console_cp());
  ASSERT_EQ(cpUtf8, frontend->get_console_output_cp());

  ASSERT_TRUE(frontend->set_console_cp(cpUsAscii));
  ASSERT_EQ(cpUsAscii, frontend->get_console_cp());
  ASSERT_EQ(cpUtf8, frontend->get_console_output_cp());

  ASSERT_TRUE(frontend->set_console_output_cp(cpUsAscii));
  ASSERT_EQ(cpUsAscii, frontend->get_console_cp());
  ASSERT_EQ(cpUsAscii, frontend->get_console_output_cp());
}

CONBACK_TEST(conback, title_a) {
  CONBACK_TEST_PREAMBLE();

  ansi_cstr_t letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  ASSERT_TRUE(frontend->set_console_title_a(letters));
  char buf[1024];
  tclib::Blob bufblob(buf, 1024);

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_a(buf, 1024));
  ASSERT_C_STREQ(letters, buf);

  bufblob.fill(-1);
  ASSERT_EQ(0, frontend->get_console_title_a(buf, 0));
  ASSERT_EQ(-1, buf[0]);

  bufblob.fill(-1);
  ASSERT_EQ(0, frontend->get_console_title_a(buf, 16));
  ASSERT_EQ(0, buf[0]);
  bufblob.fill(-1);
  ASSERT_EQ(0, frontend->get_console_title_a(buf, 25));
  ASSERT_EQ(0, buf[0]);

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_a(buf, 26));
  ASSERT_C_STREQ("ABCDEFGHIJKLMNOPQRSTUVWXY", buf);

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_a(buf, 27));
  ASSERT_C_STREQ(letters, buf);
}

CONBACK_TEST(conback, title_w) {
  CONBACK_TEST_PREAMBLE();

  const wide_char_t letters[27] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
      'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
      'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
      'Y', 'Z', 0
  };
  wide_char_t wide_minus_1 = static_cast<wide_char_t>(-1);
  ASSERT_TRUE(frontend->set_console_title_w(letters));
  wide_char_t buf[1024];
  tclib::Blob bufblob(buf, sizeof(buf));

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_w(buf, 54));
  ASSERT_BLOBEQ(StringUtils::as_blob(letters, true),
      StringUtils::as_blob(buf, true));
  ASSERT_EQ(wide_minus_1, buf[27]);

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_w(buf, 60));
  ASSERT_BLOBEQ(StringUtils::as_blob(letters, true),
      StringUtils::as_blob(buf, true));
  ASSERT_EQ(wide_minus_1, buf[27]);

  bufblob.fill(-1);
  ASSERT_EQ(0, frontend->get_console_title_w(buf, 0));
  ASSERT_EQ(wide_minus_1, buf[0]);

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_w(buf, 32));
  ASSERT_BLOBEQ(tclib::Blob(letters, 15 * sizeof(wide_char_t)),
      StringUtils::as_blob(buf));
  ASSERT_EQ(wide_minus_1, buf[17]);

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_w(buf, 50));
  ASSERT_BLOBEQ(tclib::Blob(letters, 24 * sizeof(wide_char_t)),
      StringUtils::as_blob(buf));
  ASSERT_EQ(wide_minus_1, buf[26]);

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_w(buf, 52));
  ASSERT_BLOBEQ(tclib::Blob(letters, 25 * sizeof(wide_char_t)),
      StringUtils::as_blob(buf));
  ASSERT_EQ(wide_minus_1, buf[27]);
}

CONBACK_TEST(conback, title_aw_ascii) {
  CONBACK_TEST_PREAMBLE();

  const wide_char_t wide_alphabet[27] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
      'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
      'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
      'Y', 'Z', 0
  };
  const ansi_char_t ansi_alphabet[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  wide_char_t widebuf[1024];
  tclib::Blob wideblob(widebuf, sizeof(widebuf));
  ansi_char_t ansibuf[1024];
  tclib::Blob ansiblob(ansibuf, sizeof(ansibuf));

  ASSERT_TRUE(frontend->set_console_title_a(ansi_alphabet));

  ansiblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_a(ansibuf, sizeof(ansibuf)));
  ASSERT_C_STREQ(ansi_alphabet, ansibuf);

  wideblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_w(widebuf, sizeof(widebuf)));
  ASSERT_BLOBEQ(StringUtils::as_blob(wide_alphabet, false),
      StringUtils::as_blob(widebuf, false));

  ASSERT_TRUE(frontend->set_console_title_w(wide_alphabet));

  ansiblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_a(ansibuf, sizeof(ansibuf)));
  ASSERT_C_STREQ(ansi_alphabet, ansibuf);

  wideblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_w(widebuf, sizeof(widebuf)));
  ASSERT_BLOBEQ(StringUtils::as_blob(wide_alphabet, false),
      StringUtils::as_blob(widebuf, false));
}

CONBACK_TEST(conback, title_aw_unicode) {
  CONBACK_TEST_PREAMBLE();

  // Ἀλέξ Alex Алекс
  const wide_char_t wide_names[16] = {
      0x1F08, 0x03BB, 0x03AD, 0x03BE, ' ', 'A', 'l', 'e',
      'x', ' ', 0x0410, 0x043B, 0x0435, 0x043A, 0x0441, 0x0000
  };

  wide_char_t widebuf[1024];
  tclib::Blob wideblob(widebuf, sizeof(widebuf));
  ansi_char_t ansibuf[1024];
  tclib::Blob ansiblob(ansibuf, sizeof(ansibuf));

  ASSERT_TRUE(frontend->set_console_title_w(wide_names));

  ansiblob.fill(-1);
  ASSERT_EQ(15, frontend->get_console_title_a(ansibuf, sizeof(ansibuf)));
  ASSERT_C_STREQ("???? Alex ?????", ansibuf);

  wideblob.fill(-1);
  ASSERT_EQ(15, frontend->get_console_title_w(widebuf, sizeof(widebuf)));
  ASSERT_BLOBEQ(StringUtils::as_blob(wide_names, false),
      StringUtils::as_blob(widebuf, false));

  // Surrogate pair, for 0x2A601, Cantonese "to bite". We expect it to not be
  // understood but treated as two different characters.
  const wide_char_t wide_surrogate[5] = {'{', 0xD869, 0xDE01, '}', 0};
  ASSERT_TRUE(frontend->set_console_title_w(wide_surrogate));

  ansiblob.fill(-1);
  ASSERT_EQ(4, frontend->get_console_title_a(ansibuf, sizeof(ansibuf)));
  ASSERT_C_STREQ("{??}", ansibuf);

  wideblob.fill(-1);
  ASSERT_EQ(4, frontend->get_console_title_w(widebuf, sizeof(widebuf)));
  ASSERT_BLOBEQ(StringUtils::as_blob(wide_surrogate, false),
      StringUtils::as_blob(widebuf, false));
}

CONBACK_TEST(conback, title_aw_complete) {
  CONBACK_TEST_PREAMBLE();

  ansi_char_t all_chars[256];
  for (size_t i = 0; i < 256; i++)
    all_chars[i] = static_cast<ansi_char_t>(i + 1);
  all_chars[255] = 0;

  ASSERT_TRUE(frontend->set_console_title_a(all_chars));

  wide_char_t wide_chars[256];
  ASSERT_EQ(255, frontend->get_console_title_w(wide_chars, sizeof(wide_chars)));
  for (size_t i = 0; i < 256; i++)
    ASSERT_EQ(MsDosCodec::ansi_to_wide_char(all_chars[i]), wide_chars[i]);

  ASSERT_TRUE(frontend->set_console_title_w(wide_chars));

  ansi_char_t ansi_chars[256];
  ASSERT_EQ(255, frontend->get_console_title_a(ansi_chars, sizeof(ansi_chars)));
  for (size_t i = 0; i < 256; i++)
    ASSERT_EQ(all_chars[i], ansi_chars[i]);
}
