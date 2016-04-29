//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

#include "conback-utils.hh"

using namespace conprx;
using namespace tclib;

CONBACK_TEST(conback, read_console) {
  CONBACK_TEST_PREAMBLE();
  def_ref_t<ConsoleFrontend> native_frontend;
  def_ref_t<ConsolePlatform> native_platform;
  def_ref_t<WinTty> wty;
  if (!use_native) {
    native_frontend = ConsoleFrontend::new_native();
    native_platform = ConsolePlatform::new_native();
    wty = WinTty::new_adapted(*native_frontend, *native_platform);
    __multiplexer__.set_wty(*wty);
  }

  LOG_INFO("[read_console_a] Type foo<enter>");
  handle_t std_in = platform->get_std_handle(kStdInputHandle);
  ansi_char_t abuf[128];
  memset(abuf, -1, 128);
  dword_t chars_read = 0;
  frontend->read_console_a(std_in, abuf, 128, &chars_read, NULL);
  ASSERT_EQ(5, chars_read);
  ASSERT_BLOBEQ(tclib::Blob("foo\x0d\x0a\xff", 6), tclib::Blob(abuf, 6));

  LOG_INFO("[read_console_w] Type foo<enter>");
  wide_char_t wbuf[128];
  memset(wbuf, -1, 128 * sizeof(wide_char_t));
  chars_read = 0;
  frontend->read_console_w(std_in, wbuf, 128, &chars_read, NULL);
  ASSERT_EQ(5, chars_read);
  wide_char_t wide_foo[6] = {'f', 'o', 'o', 0x0d, 0x0a, 0xffff};
  ASSERT_BLOBEQ(tclib::Blob(wide_foo, sizeof(wide_foo)), tclib::Blob(wbuf, 6 * sizeof(wide_char_t)));
}
