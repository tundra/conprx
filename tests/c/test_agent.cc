//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver-manager.hh"
#include "test.hh"

BEGIN_C_INCLUDES
#include "utils/misc-inl.h"
#include "utils/strbuf.h"
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;
using namespace conprx;

MULTITEST(agent, simple, bool, use_fake, ("fake", true), ("real", false)) {
  if (!use_fake && !DriverManager::kSupportsRealAgent)
    SKIP_TEST(kIsMsvc ? "debug codegen" : "msvc only");
  DriverManager driver;
  driver.set_agent_type(use_fake ? DriverManager::atFake : DriverManager::atReal);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  DriverRequest echo0 = driver.echo(5436);
  ASSERT_EQ(5436, echo0->integer_value());

  ASSERT_F_TRUE(driver.join(NULL));
}

TEST(agent, inject_fail) {
  if (!DriverManager::kSupportsRealAgent)
    SKIP_TEST("requires real agent");
  DriverManager driver;
  driver.set_agent_type(DriverManager::atReal);
  driver.set_agent_path(new_c_string("flip-flap-foobeliboo"));
  driver.set_silence_log(true);
  log_o *old_log = silence_global_log();
  ASSERT_FALSE(driver.start());
  set_global_log(old_log);
  ASSERT_F_TRUE(driver->ensure_process_resumed());
  int exit_code = 0;
  ASSERT_F_TRUE(driver.join(&exit_code));
  ASSERT_EQ(1, exit_code);
}

class PokeCounter : public BasicConsoleBackend {
public:
  PokeCounter()  : poke_count(0) { }
  virtual response_t<int64_t> poke(int64_t value);
  size_t poke_count;
};

response_t<int64_t> PokeCounter::poke(int64_t value) {
  poke_count++;
  return response_t<int64_t>::of(value + 257);
}

TEST(agent, simulate_roundtrip) {
  DriverManager driver;
  driver.set_agent_type(DriverManager::atFake);
  driver.set_frontend_type(dfSimulating);
  PokeCounter counter;
  driver.set_backend(&counter);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  ASSERT_EQ(0, counter.poke_count);
  DriverRequest poke0 = driver.poke_backend(4212);
  ASSERT_EQ(4212 + 257, poke0->integer_value());
  ASSERT_EQ(1, counter.poke_count);
  DriverRequest poke1 = driver.poke_backend(5457);
  ASSERT_EQ(5457 + 257, poke1->integer_value());
  ASSERT_EQ(2, counter.poke_count);

  ASSERT_F_TRUE(driver.join(NULL));
}

class CodePageBackend : public BasicConsoleBackend {
public:
  virtual response_t<uint32_t> get_console_cp(bool is_output);
  virtual response_t<bool_t> set_console_cp(uint32_t value, bool is_output);
  response_t<uint32_t> input;
  response_t<uint32_t> output;
};

response_t<uint32_t> CodePageBackend::get_console_cp(bool is_output) {
  return is_output ? output : input;
}

response_t<bool_t> CodePageBackend::set_console_cp(uint32_t value, bool is_output) {
  (is_output ? output : input) = response_t<uint32_t>::of(value);
  return response_t<bool_t>::yes();
}


#define SKIP_IF_UNSUPPORTED(USE_REAL) do {                                     \
  if ((USE_REAL) && !DriverManager::kSupportsRealAgent)                        \
    SKIP_TEST("requires real agent");                                          \
} while (false)

static void configure_driver(DriverManager *driver, bool use_real) {
  if (use_real) {
    driver->set_agent_type(DriverManager::atReal);
    driver->set_frontend_type(dfNative);
  } else {
    driver->set_agent_type(DriverManager::atFake);
    driver->set_frontend_type(dfSimulating);
  }
}

#include "agent/lpc.hh"

MULTITEST(agent, native_get_cp, bool, use_real, ("real", true), ("simul", false)) {
  SKIP_IF_UNSUPPORTED(use_real);
  CodePageBackend backend;
  DriverManager driver;
  driver.set_backend(&backend);
  configure_driver(&driver, use_real);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  backend.input = response_t<uint32_t>::of(82723);
  DriverRequest cp0 = driver.get_console_cp();
  ASSERT_EQ(82723, cp0->integer_value());

  backend.input = response_t<uint32_t>::of(32728);
  DriverRequest cp1 = driver.get_console_cp();
  ASSERT_EQ(32728, cp1->integer_value());

  backend.input = response_t<uint32_t>::error(123);
  DriverRequest cp2 = driver.get_console_cp();
  ASSERT_EQ(123, static_cast<int32_t>(cp2.error().native_as<ConsoleError>()->code()));

  ASSERT_F_TRUE(driver.join(NULL));
}

MULTITEST(agent, native_set_cp, bool, use_real, ("real", true), ("simul", false)) {
  SKIP_IF_UNSUPPORTED(use_real);
  CodePageBackend backend;
  DriverManager driver;
  driver.set_backend(&backend);
  configure_driver(&driver, use_real);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  backend.input = response_t<uint32_t>::of(82723);
  DriverRequest cp0 = driver.get_console_cp();
  ASSERT_EQ(82723, cp0->integer_value());

  DriverRequest cp1 = driver.set_console_cp(54643);
  ASSERT_TRUE(cp1->bool_value());
  ASSERT_EQ(54643, backend.input.value());

  ASSERT_F_TRUE(driver.join(NULL));
}

MULTITEST(agent, native_set_output_cp, bool, use_real, ("real", true), ("simul", false)) {
  SKIP_IF_UNSUPPORTED(use_real);
  CodePageBackend backend;
  DriverManager driver;
  driver.set_backend(&backend);
  configure_driver(&driver, use_real);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  backend.output = response_t<uint32_t>::of(534);
  backend.input = response_t<uint32_t>::of(653);
  DriverRequest cp0 = driver.get_console_output_cp();
  ASSERT_EQ(534, cp0->integer_value());
  DriverRequest cp1 = driver.get_console_cp();
  ASSERT_EQ(653, cp1->integer_value());

  DriverRequest cp2 = driver.set_console_output_cp(443);
  ASSERT_TRUE(cp2->bool_value());
  ASSERT_EQ(443, backend.output.value());

  DriverRequest cp3 = driver.get_console_output_cp();
  ASSERT_EQ(443, cp3->integer_value());
  DriverRequest cp4 = driver.get_console_cp();
  ASSERT_EQ(653, cp4->integer_value());


  ASSERT_F_TRUE(driver.join(NULL));
}

class TitleBackend : public BasicConsoleBackend {
public:
  TitleBackend() : is_unicode(false) { }
  ~TitleBackend();
  virtual response_t<bool_t> set_console_title(tclib::Blob title, bool is_unicode);
  virtual response_t<uint32_t> get_console_title(tclib::Blob buffer, bool is_unicode);
  void set_title(const char *new_title, bool new_is_unicode);
  utf8_t c_str();

  tclib::Blob title;
  bool is_unicode;
};

static tclib::Blob clone_blob(tclib::Blob blob) {
  tclib::Blob result = allocator_default_malloc(blob.size());
  blob_copy_to(blob, result);
  return result;
}

TitleBackend::~TitleBackend() {
  allocator_default_free(title);
}

void TitleBackend::set_title(const char *new_title, bool new_is_unicode) {
  set_console_title(tclib::Blob(new_title, strlen(new_title)), new_is_unicode);
}

utf8_t TitleBackend::c_str() {
  return new_string(static_cast<char*>(title.start()), title.size());
}

response_t<bool_t> TitleBackend::set_console_title(tclib::Blob new_title, bool new_is_unicode) {
  allocator_default_free(title);
  is_unicode = new_is_unicode;
  title = clone_blob(new_title);
  return response_t<bool_t>::yes();
}

response_t<uint32_t> TitleBackend::get_console_title(tclib::Blob buffer, bool is_unicode) {
  uint32_t len = static_cast<uint32_t>(min_size(buffer.size(), title.size()));
  tclib::Blob data(title.start(), len);
  blob_copy_to(data, buffer);
  return response_t<uint32_t>::of(len);
}

#define ASSERT_V_STREQ(C_STR, VAR)                                             \
  ASSERT_STREQ(new_c_string(C_STR), new_string((VAR).string_chars(), (VAR).string_length()))

MULTITEST(agent, native_set_title, bool, use_real, ("real", true), ("simul", false)) {
  SKIP_IF_UNSUPPORTED(use_real);
  TitleBackend backend;
  DriverManager driver;
  configure_driver(&driver, use_real);
  driver.set_backend(&backend);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  backend.set_title("First", true);
  ASSERT_STREQ(new_c_string("First"), backend.c_str());
  ASSERT_TRUE(backend.is_unicode);

  DriverRequest gt0 = driver.get_console_title_a(256);
  ASSERT_V_STREQ("First", *gt0);

  DriverRequest st0 = driver.set_console_title_a("Second");
  ASSERT_TRUE(st0->bool_value());
  ASSERT_STREQ(new_c_string("Second"), backend.c_str());
  ASSERT_FALSE(backend.is_unicode);

  DriverRequest gt1 = driver.get_console_title_a(256);
  ASSERT_V_STREQ("Second", *gt1);

  DriverRequest st1 = driver.set_console_title_a("Third");
  ASSERT_TRUE(st1->bool_value());
  ASSERT_STREQ(new_c_string("Third"), backend.c_str());
  ASSERT_FALSE(backend.is_unicode);

  DriverRequest gt2 = driver.get_console_title_a(256);
  ASSERT_V_STREQ("Third", *gt2);

  ASSERT_F_TRUE(driver.join(NULL));
}

MULTITEST(agent, set_std_modes, bool, use_real, ("real", true), ("simul", false)) {
  SKIP_IF_UNSUPPORTED(use_real);
  BasicConsoleBackend backend;
  DriverManager driver;
  configure_driver(&driver, use_real);
  driver.set_backend(&backend);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  DriverRequest gsh0 = driver.get_std_handle(conprx::kStdInputHandle);
  Handle input = *gsh0->native_as<Handle>();
  DriverRequest gcm0 = driver.get_console_mode(input);
  ASSERT_TRUE(gcm0->is_integer());

  ASSERT_TRUE(driver.set_console_mode(input, 0xF00B00)->bool_value());
  ASSERT_EQ(0xF00B00, driver.get_console_mode(input)->integer_value());

  ASSERT_F_TRUE(driver.join(NULL));
}

class InfoBackend : public BasicConsoleBackend {
public:
  virtual response_t<bool_t> get_console_screen_buffer_info(Handle buffer,
      console_screen_buffer_infoex_t *info_out);
};

response_t<bool_t> InfoBackend::get_console_screen_buffer_info(Handle buffer,
      console_screen_buffer_infoex_t *info_out) {
  info_out->dwSize.X = 0x0FEE;
  info_out->dwSize.Y = 0x0BAA;
  info_out->dwCursorPosition.X = 0x0EFF;
  info_out->dwCursorPosition.Y = 0x0ABB;
  info_out->wAttributes = 0x0CAB;
  info_out->dwMaximumWindowSize.X = 0x0F00;
  info_out->dwMaximumWindowSize.Y = 0x0BAD;
  info_out->srWindow.Left = 0x0DAB;
  info_out->srWindow.Top = 0x0ABD;
  info_out->srWindow.Right = 0x0BAB;
  info_out->srWindow.Bottom = 0x0D0B;
  info_out->wPopupAttributes = 0x0B00;
  info_out->bFullscreenSupported = true;
  for (size_t i = 0; i < 16; i++)
    info_out->ColorTable[i] = static_cast<colorref_t>(0x0DECADE0 + i);
  return response_t<bool_t>::yes();
}

MULTITEST(agent, get_info, bool, use_real, ("real", true), ("simul", false)) {
  SKIP_IF_UNSUPPORTED(use_real);
  InfoBackend backend;
  DriverManager driver;
  configure_driver(&driver, use_real);
  driver.set_backend(&backend);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  DriverRequest gsh0 = driver.get_std_handle(conprx::kStdOutputHandle);
  Handle output = *gsh0->native_as<Handle>();
  DriverRequest gin0 = driver.get_console_screen_buffer_info(output);
  console_screen_buffer_info_t *i0 = gin0->native_as<console_screen_buffer_info_t>();
  ASSERT_TRUE(i0 != NULL);
  ASSERT_EQ(0x0FEE, i0->dwSize.X);
  ASSERT_EQ(0x0BAA, i0->dwSize.Y);
  ASSERT_EQ(0x0EFF, i0->dwCursorPosition.X);
  ASSERT_EQ(0x0ABB, i0->dwCursorPosition.Y);
  ASSERT_EQ(0x0CAB, i0->wAttributes);
  ASSERT_EQ(0x0F00, i0->dwMaximumWindowSize.X);
  ASSERT_EQ(0x0BAD, i0->dwMaximumWindowSize.Y);
  ASSERT_EQ(0x0DAB, i0->srWindow.Left);
  ASSERT_EQ(0x0ABD, i0->srWindow.Top);
  ASSERT_EQ(0x0BAB, i0->srWindow.Right);
  ASSERT_EQ(0x0D0B, i0->srWindow.Bottom);

  DriverRequest gin1 = driver.get_console_screen_buffer_info_ex(output);
  console_screen_buffer_infoex_t *i1 = gin1->native_as<console_screen_buffer_infoex_t>();
  ASSERT_TRUE(i1 != NULL);
  ASSERT_EQ(0x0FEE, i1->dwSize.X);
  ASSERT_EQ(0x0BAA, i1->dwSize.Y);
  ASSERT_EQ(0x0EFF, i1->dwCursorPosition.X);
  ASSERT_EQ(0x0ABB, i1->dwCursorPosition.Y);
  ASSERT_EQ(0x0CAB, i1->wAttributes);
  ASSERT_EQ(0x0F00, i1->dwMaximumWindowSize.X);
  ASSERT_EQ(0x0BAD, i1->dwMaximumWindowSize.Y);
  ASSERT_EQ(0x0DAB, i1->srWindow.Left);
  ASSERT_EQ(0x0ABD, i1->srWindow.Top);
  ASSERT_EQ(0x0BAB, i1->srWindow.Right);
  ASSERT_EQ(0x0D0B, i1->srWindow.Bottom);
  ASSERT_EQ(0x0B00, i1->wPopupAttributes);
  ASSERT_TRUE(i1->bFullscreenSupported);
  for (size_t i = 0; i < 16; i++)
    ASSERT_EQ(0x0DECADE0 + i, i1->ColorTable[i]);

  ASSERT_F_TRUE(driver.join(NULL));
}

// A backend that fails on *everything*. Don't forget to add a test when you
// add a new method.
class FailingConsoleBackend : public ConsoleBackend {
public:
  FailingConsoleBackend(uint32_t a = 1, uint32_t b = 1) : a_(a), b_(b) { }
  response_t<int64_t> poke(int64_t value) { return fail<int64_t>(); }
  response_t<uint32_t> get_console_cp(bool is_output) { return fail<uint32_t>(); }
  response_t<bool_t> set_console_cp(uint32_t value, bool is_output) { return fail<bool_t>(); }
  response_t<uint32_t> get_console_title(tclib::Blob buffer, bool is_unicode) { return fail<uint32_t>(); }
  response_t<bool_t> set_console_title(tclib::Blob title, bool is_unicode) { return fail<bool_t>(); }
  response_t<uint32_t> get_console_mode(Handle handle) { return fail<uint32_t>(); }
  response_t<bool_t> set_console_mode(Handle handle, uint32_t mode) { return fail<bool_t>(); }
  response_t<bool_t> get_console_screen_buffer_info(Handle buffer, console_screen_buffer_infoex_t *info_out) { return fail<bool_t>(); }

  // Returns the next error code in the sequence.
  uint32_t gen_error();

private:
  template <typename T>
  response_t<T> fail() { return response_t<T>::error(gen_error()); }
  uint32_t a_;
  uint32_t b_;
};

uint32_t FailingConsoleBackend::gen_error() {
  uint32_t result = a_;
  a_ = b_;
  b_ = result + a_;
  return result;
}

static int64_t get_last_error(const DriverRequest &request) {
  // Casting away the const here is super cheating but it would be sooo tedious
  // to create a whole set of parallel const methods to be able to do this
  // const.
  Variant error = const_cast<DriverRequest&>(request).error();
  return error.native_as<ConsoleError>()->code();
}

MULTITEST(agent, failures, bool, use_real, ("real", true), ("simul", false)) {
  SKIP_IF_UNSUPPORTED(use_real);
  FailingConsoleBackend backend(4, 7);
  DriverManager driver;
  configure_driver(&driver, use_real);
  driver.set_backend(&backend);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  // In the simulated version the values from the backend. In the real version
  // it's the negated arguments. In both cases the values are the same.
  ASSERT_EQ(4, get_last_error(driver.poke_backend(-4)));
  ASSERT_EQ(7, get_last_error(driver.poke_backend(-7)));

  if (use_real) {
    // If we're using the real console there's no backend so we need to advance
    // it manually for the codes to match up.
    ASSERT_EQ(4, backend.gen_error());
    ASSERT_EQ(7, backend.gen_error());
  }

  ASSERT_EQ(11, get_last_error(driver.get_console_cp()));
  ASSERT_EQ(18, get_last_error(driver.get_console_output_cp()));

  ASSERT_EQ(29, get_last_error(driver.set_console_cp(10)));
  ASSERT_EQ(47, get_last_error(driver.set_console_output_cp(10)));

  ASSERT_EQ(76, get_last_error(driver.get_console_title_a(1)));
  ASSERT_EQ(123, get_last_error(driver.set_console_title_a("foo")));
  Handle dummy_handle(10);
  ASSERT_EQ(199, get_last_error(driver.get_console_mode(dummy_handle)));
  ASSERT_EQ(322, get_last_error(driver.set_console_mode(dummy_handle, 0xFF)));

  ASSERT_EQ(521, get_last_error(driver.get_console_screen_buffer_info(dummy_handle)));

  ASSERT_F_TRUE(driver.join(NULL));
}
