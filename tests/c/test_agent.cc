//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver-manager.hh"
#include "test.hh"
#include "utils/string.hh"

BEGIN_C_INCLUDES
#include "utils/misc-inl.h"
#include "utils/strbuf.h"
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;
using namespace conprx;

// Declares a standard agent test that is run both against the real agent and
// the simulated one.
#define AGENT_TEST(NAME) MULTITEST(agent, NAME, bool, use_real, ("real", true), ("simul", false))

// Helper class that takes care of joining drivers if they are started properly.
class DriverManagerJoiner {
public:
  DriverManagerJoiner() : manager_(NULL) { }
  ~DriverManagerJoiner();
  void set_driver(DriverManager *manager) { manager_ = manager; }
private:
  DriverManager *manager_;
};

DriverManagerJoiner::~DriverManagerJoiner() {
  if (manager_ == NULL)
    return;
  ASSERT_F_TRUE(manager_->join(NULL));
}

// Initializes the driver to use for this agent test.
#define AGENT_TEST_PREAMBLE(BACKEND, USE_REAL)                                 \
    if ((USE_REAL) && !DriverManager::kSupportsRealAgent)                      \
      SKIP_TEST("requires real agent");                                        \
    DriverManager driver;                                                      \
    DriverManagerJoiner joiner;                                                \
    do {                                                                       \
      configure_driver(&driver, (USE_REAL));                                   \
      driver.set_backend((BACKEND));                                           \
      ASSERT_F_TRUE(driver.start());                                           \
      ASSERT_F_TRUE(driver.connect());                                         \
      joiner.set_driver(&driver);                                              \
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

AGENT_TEST(native_get_cp) {
  CodePageBackend backend;
  AGENT_TEST_PREAMBLE(&backend, use_real);

  backend.input = response_t<uint32_t>::of(82723);
  DriverRequest cp0 = driver.get_console_cp();
  ASSERT_EQ(82723, cp0->integer_value());

  backend.input = response_t<uint32_t>::of(32728);
  DriverRequest cp1 = driver.get_console_cp();
  ASSERT_EQ(32728, cp1->integer_value());

  backend.input = response_t<uint32_t>::error(123);
  DriverRequest cp2 = driver.get_console_cp();
  ASSERT_EQ(123, static_cast<int32_t>(cp2.error().native_as<ConsoleError>()->code()));
}

AGENT_TEST(native_set_cp) {
  CodePageBackend backend;
  AGENT_TEST_PREAMBLE(&backend, use_real);

  backend.input = response_t<uint32_t>::of(82723);
  DriverRequest cp0 = driver.get_console_cp();
  ASSERT_EQ(82723, cp0->integer_value());

  DriverRequest cp1 = driver.set_console_cp(54643);
  ASSERT_TRUE(cp1->bool_value());
  ASSERT_EQ(54643, backend.input.value());
}

AGENT_TEST(native_set_output_cp) {
  CodePageBackend backend;
  AGENT_TEST_PREAMBLE(&backend, use_real);

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
}

class TitleBackend : public BasicConsoleBackend {
public:
  TitleBackend() : is_unicode(false) { }
  ~TitleBackend();
  virtual response_t<bool_t> set_console_title(tclib::Blob title, bool is_unicode);
  virtual response_t<uint32_t> get_console_title(tclib::Blob buffer, bool is_unicode,
      size_t *bytes_written_out);
  void set_title(ansi_cstr_t new_title);
  void set_title(wide_cstr_t new_title);
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

void TitleBackend::set_title(ansi_cstr_t new_title) {
  set_console_title(StringUtils::as_blob(new_title), false);
}

void TitleBackend::set_title(wide_cstr_t new_title) {
  set_console_title(StringUtils::as_blob(new_title), true);
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

response_t<uint32_t> TitleBackend::get_console_title(tclib::Blob buffer, bool is_unicode,
    size_t *bytes_written_out) {
  uint32_t len = static_cast<uint32_t>(min_size(buffer.size(), title.size()));
  tclib::Blob data(title.start(), len);
  blob_copy_to(data, buffer);
  *bytes_written_out = len;
  return response_t<uint32_t>::of(len);
}

#define ASSERT_V_STREQ(C_STR, VAR)                                             \
  ASSERT_STREQ(new_c_string(C_STR), new_string(static_cast<const char *>((VAR).blob_data()), (VAR).blob_size()))

static tclib::Blob as_blob(Variant var) {
  return tclib::Blob(var.blob_data(), var.blob_size());
}

AGENT_TEST(native_set_title_noconv) {
  TitleBackend backend;
  AGENT_TEST_PREAMBLE(&backend, use_real);

  backend.set_title("First");
  ASSERT_STREQ(new_c_string("First"), backend.c_str());
  ASSERT_FALSE(backend.is_unicode);

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

  wide_char_t fourth[] = {'f', 'o', 'u', 'r', 't', 'h', 0};
  DriverRequest st2 = driver.set_console_title_w(fourth);
  ASSERT_TRUE(st2->bool_value());

  DriverRequest gt3 = driver.get_console_title_w(256);
  ASSERT_BLOBEQ(StringUtils::as_blob(fourth), as_blob(*gt3));
}

AGENT_TEST(set_std_modes) {
  BasicConsoleBackend backend;
  AGENT_TEST_PREAMBLE(&backend, use_real);

  DriverRequest gsh0 = driver.get_std_handle(conprx::kStdInputHandle);
  Handle input = *gsh0->native_as<Handle>();

  if (!input.is_console())
    // TODO: ensure that stdin is a console. Maybe allocate a fresh console
    //   for the driver?
    SKIP_TEST("stdin not console");

  DriverRequest gcm0 = driver.get_console_mode(input);
  ASSERT_TRUE(gcm0->is_integer());

  uint32_t old_mode = static_cast<uint32_t>(driver.get_console_mode(input)->integer_value());
  uint32_t enable_mouse_input_mode = 0x0010;
  uint32_t new_mode = old_mode ^ enable_mouse_input_mode;

  ASSERT_TRUE(driver.set_console_mode(input, new_mode)->bool_value());
  ASSERT_EQ(new_mode, backend.get_handle_shadow(input).mode());
  ASSERT_EQ(new_mode, driver.get_console_mode(input)->integer_value());

  ASSERT_TRUE(driver.set_console_mode(input, old_mode)->bool_value());
  ASSERT_EQ(old_mode, backend.get_handle_shadow(input).mode());
  ASSERT_EQ(old_mode, driver.get_console_mode(input)->integer_value());
}

class InfoBackend : public BasicConsoleBackend {
public:
  virtual response_t<bool_t> get_console_screen_buffer_info(Handle buffer,
      ScreenBufferInfo *info_out);
};

response_t<bool_t> InfoBackend::get_console_screen_buffer_info(Handle buffer,
    ScreenBufferInfo *info_out) {
  info_out->set_size(coord_new(0x0FEE, 0x0BAA));
  info_out->set_cursor_position(coord_new(0x0EFF, 0x0ABB));
  info_out->set_attributes(0x0CAB);
  info_out->set_maximum_window_size(coord_new(0x0F00, 0x0BAD));
  info_out->set_window(small_rect_new(0x0DAB, 0x0ABD, 0x0BAB, 0x0D0B));
  info_out->set_popup_attributes(0x0B00);
  info_out->set_fullscreen_supported(true);
  for (size_t i = 0; i < 16; i++)
    info_out->color_table()[i] = static_cast<colorref_t>(0x0DECADE0 + i);
  return response_t<bool_t>::yes();
}

AGENT_TEST(get_info) {
  InfoBackend backend;
  AGENT_TEST_PREAMBLE(&backend, use_real);

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
}

// Helper that makes it easy to build a list of integer ranges to use as test
// cases. Honestly it's gratuitously, unnecessarily general with the whole
// quadratic/cubic thing but I just needed to take the opportunity to implement
// a difference engine when I could.
class CaseMaker {
public:
  typedef std::vector<size_t>::iterator iterator;
  iterator begin() { return entries_.begin(); }
  iterator end() { return entries_.end(); }

  // Adds a linear range of cases, starting with first, then next, and then
  // continuing at the same interval until reaching the limit. The limit itself
  // is not added.
  void add_cases(size_t a, size_t b, size_t limit);

  // Adds a quadratic range.
  void add_cases(size_t a, size_t b, size_t c, size_t limit);

  // Adds a cubic range.
  void add_cases(size_t a, size_t b, size_t c, size_t d, size_t limit);

  // Adds an individual case.
  void add_case(size_t value) { entries_.push_back(value); }

private:
  void run_difference_engine(size_t diffc, size_t *diffv, size_t limit);

  std::vector<size_t> entries_;
};

void CaseMaker::add_cases(size_t a, size_t b, size_t limit) {
  size_t valv[2] = {a, b};
  run_difference_engine(2, valv, limit);
}

void CaseMaker::add_cases(size_t a, size_t b, size_t c, size_t limit) {
  size_t valv[3] = {a, b, c};
  run_difference_engine(3, valv, limit);
}

void CaseMaker::add_cases(size_t a, size_t b, size_t c, size_t d, size_t limit) {
  size_t valv[4] = {a, b, c, d};
  run_difference_engine(4, valv, limit);
}

void CaseMaker::run_difference_engine(size_t valc, size_t *valv, size_t limit) {
  ASSERT_REL(valc, <=, 16);
  // Calculate the n'th order differences.
  size_t diffa[16];
  size_t diffb[16];
  memcpy(diffa, valv, sizeof(size_t) * valc);
  size_t diffv[16];
  for (size_t io = 0; io < valc; io++) {
    size_t *from = ((io & 1) == 0) ? diffa : diffb;
    size_t *to = ((io & 1) == 0) ? diffb : diffa;
    diffv[io] = from[0];
    for (size_t iw = 0; iw < (valc - io); iw++) {
      to[iw] = from[iw + 1] - from[iw];
    }
  }
  // Then run the difference calculation!
  while (diffv[0] < limit) {
    add_case(diffv[0]);
    for (size_t i = 0; i < valc - 1; i++)
      diffv[i] += diffv[i + 1];
  }
}

// Adds test cases that cover the range between 0 and 255 with special emphasis
// around the message inline limits, 80 (ansi chars) and 40 (wide).
static void add_inline_limit_cases(CaseMaker *cases) {
  cases->add_cases(0, 5, 35); // Skim over the short strings
  cases->add_cases(35, 36, 45); // Slowly over the first inline/noninline boundary
  cases->add_cases(45, 50, 75); // Skim
  cases->add_cases(75, 76, 85); // Slowly over the second inline/noninline boundary
  cases->add_cases(85, 92, 255); // Fast to the end
  cases->add_case(255); // Remember the boundary.
}

class WriteConsoleBackend : public BasicConsoleBackend {
public:
  WriteConsoleBackend() : last_written(ucs16_empty()) { }
  ~WriteConsoleBackend() { ucs16_default_delete(last_written); }
  response_t<uint32_t> write_console(Handle output, tclib::Blob data, bool is_unicode);
  ucs16_t last_written;
};

response_t<uint32_t> WriteConsoleBackend::write_console(Handle output,
    tclib::Blob data, bool is_unicode) {
  ucs16_default_delete(last_written);
  last_written = blob_to_ucs16(data, is_unicode);
  return response_t<uint32_t>::of(static_cast<uint32_t>(data.size()));
}

AGENT_TEST(write_console_aw) {
  WriteConsoleBackend backend;
  AGENT_TEST_PREAMBLE(&backend, use_real);

  DriverRequest gsh0 = driver.get_std_handle(conprx::kStdOutputHandle);
  Handle output = *gsh0->native_as<Handle>();
  ASSERT_EQ(0, backend.last_written.length);

  ansi_char_t ansi_chars[256];
  wide_char_t wide_chars[256];
  for (size_t i = 0; i < 255; i++) {
    ansi_char_t c = static_cast<ansi_char_t>('A' + (i % 26));
    ansi_chars[i] = c;
    wide_chars[i] = c;
  }
  wide_chars[255] = ansi_chars[255] = 0;

  CaseMaker cases;
  add_inline_limit_cases(&cases);

  for (CaseMaker::iterator i = cases.begin(); i != cases.end(); i++) {
    size_t chars = *i;
    size_t wide_size = chars * sizeof(wide_char_t);
    DriverRequest wca0 = driver.write_console_a(output, tclib::Blob(ansi_chars, chars));
    ASSERT_EQ(chars, wca0->integer_value());
    ASSERT_EQ(chars, backend.last_written.length);
    ASSERT_BLOBEQ(tclib::Blob(backend.last_written.chars, wide_size),
        tclib::Blob(wide_chars, wide_size));
  }

  for (CaseMaker::iterator i = cases.begin(); i != cases.end(); i++) {
    size_t chars = *i;
    size_t wide_size = chars * sizeof(wide_char_t);
    DriverRequest wcw0 = driver.write_console_w(output, tclib::Blob(wide_chars, wide_size));
    ASSERT_EQ(chars, wcw0->integer_value());
    ASSERT_EQ(chars, backend.last_written.length);
    ASSERT_BLOBEQ(tclib::Blob(backend.last_written.chars, wide_size),
        tclib::Blob(wide_chars, wide_size));
  }
}

class ReadConsoleBackend : public BasicConsoleBackend {
public:
  ReadConsoleBackend()
    : contents(ucs16_empty())
    , last_request_size(0)
    , last_is_unicode(false)
    , control_key_state(0) { }

  ~ReadConsoleBackend() { ucs16_default_delete(contents); }
  response_t<uint32_t> read_console(Handle output, tclib::Blob buffer,
      bool is_unicode, size_t *bytes_read_out, ReadConsoleControl *input_control);

  void set_contents(const char *str) {
    ucs16_default_delete(contents);
    contents = blob_to_ucs16(StringUtils::as_blob(str, false), false);
  }

  ucs16_t contents;
  size_t last_request_size;
  bool last_is_unicode;
  ReadConsoleControl last_control;
  ulong_t control_key_state;
};

response_t<uint32_t> ReadConsoleBackend::read_console(Handle output,
    tclib::Blob buffer, bool is_unicode, size_t *bytes_read_out,
    ReadConsoleControl *input_control) {
  last_request_size = buffer.size();
  last_is_unicode = is_unicode;
  last_control = *input_control;
  input_control->set_control_key_state(control_key_state);
  size_t read = ucs16_to_blob(contents, buffer, is_unicode);
  *bytes_read_out = read;
  return response_t<uint32_t>::of(static_cast<uint32_t>(read));
}

TEST(agent, case_maker) {
  CaseMaker l;
  l.add_cases(10, 12, 20);
  l.add_cases(101, 104, 109, 150);
  l.add_cases(1001, 1008, 1027, 1064, 1400);
  CaseMaker::iterator li = l.begin();
  ASSERT_EQ(10, *li); li++;
  ASSERT_EQ(12, *li); li++;
  ASSERT_EQ(14, *li); li++;
  ASSERT_EQ(16, *li); li++;
  ASSERT_EQ(18, *li); li++;
  ASSERT_EQ(101, *li); li++;
  ASSERT_EQ(104, *li); li++;
  ASSERT_EQ(109, *li); li++;
  ASSERT_EQ(116, *li); li++;
  ASSERT_EQ(125, *li); li++;
  ASSERT_EQ(136, *li); li++;
  ASSERT_EQ(149, *li); li++;
  ASSERT_EQ(1001, *li); li++;
  ASSERT_EQ(1008, *li); li++;
  ASSERT_EQ(1027, *li); li++;
  ASSERT_EQ(1064, *li); li++;
  ASSERT_EQ(1125, *li); li++;
  ASSERT_EQ(1216, *li); li++;
  ASSERT_EQ(1343, *li); li++;
  ASSERT_TRUE(li == l.end());
}

AGENT_TEST(read_console_aw) {
  ReadConsoleBackend backend;
  AGENT_TEST_PREAMBLE(&backend, use_real);

  ansi_char_t ansi_chars[256];
  wide_char_t wide_chars[256];
  for (size_t i = 0; i < 255; i++) {
    ansi_char_t c = static_cast<ansi_char_t>('A' + (i % 26));
    ansi_chars[i] = c;
    wide_chars[i] = c;
  }
  wide_chars[255] = ansi_chars[255] = 0;

  backend.set_contents(ansi_chars);

  DriverRequest gsh0 = driver.get_std_handle(conprx::kStdInputHandle);
  Handle input = *gsh0->native_as<Handle>();
  ASSERT_EQ(0, backend.last_request_size);
  ASSERT_EQ(false, backend.last_is_unicode);

  CaseMaker cases;
  cases.add_case(10);
  // add_inline_limit_cases(&cases);

  for (CaseMaker::iterator i = cases.begin(); i != cases.end(); i++) {
    uint32_t chars = static_cast<uint32_t>(*i);
    DriverRequest rca = driver.read_console_a(input, chars);
    ASSERT_EQ(chars, backend.last_request_size);
    ASSERT_EQ(false, backend.last_is_unicode);
    ASSERT_TRUE(rca->is_blob());
    ASSERT_EQ(chars, rca->blob_size());
    ASSERT_BLOBEQ(tclib::Blob(ansi_chars, chars), as_blob(*rca));
  }

  for (CaseMaker::iterator i = cases.begin(); i != cases.end(); i++) {
    uint32_t chars = static_cast<uint32_t>(*i);
    size_t size_in_bytes = chars * sizeof(wide_char_t);
    DriverRequest rcw = driver.read_console_w(input, chars);
    ASSERT_EQ(size_in_bytes, backend.last_request_size);
    ASSERT_EQ(true, backend.last_is_unicode);
    ASSERT_TRUE(rcw->is_blob());
    ASSERT_EQ(size_in_bytes, rcw->blob_size());
    ASSERT_BLOBEQ(tclib::Blob(wide_chars, size_in_bytes), as_blob(*rcw));
  }

  const char *not_long = "SHORT";
  backend.set_contents(not_long);

  DriverRequest rca0 = driver.read_console_a(input, 100);
  ASSERT_EQ(100, backend.last_request_size);
  ASSERT_EQ(false, backend.last_is_unicode);
  ASSERT_BLOBEQ(StringUtils::as_blob(not_long, false), as_blob(*rca0));

  DriverRequest rcw0 = driver.read_console_w(input, 100);
  ASSERT_EQ(200, backend.last_request_size);
  ASSERT_EQ(true, backend.last_is_unicode);
  wide_char_t not_long_wide[] = {'S', 'H', 'O', 'R', 'T', 0};
  ASSERT_BLOBEQ(StringUtils::as_blob(not_long_wide, false), as_blob(*rcw0));

  ASSERT_EQ(0, backend.last_control.control_key_state());
  ASSERT_EQ(0, backend.last_control.ctrl_wakeup_mask());
  ASSERT_EQ(0, backend.last_control.initial_chars());

  ReadConsoleControl control;
  control.set_ctrl_wakeup_mask(0xBCCB);
  control.set_initial_chars(0xA);
  control.set_control_key_state(0);
  backend.control_key_state = 0xABBA;

  DriverRequest rcw1 = driver.read_console_w(input, 100, &control);
  ASSERT_EQ(200, backend.last_request_size);
  ASSERT_EQ(true, backend.last_is_unicode);
  ASSERT_BLOBEQ(StringUtils::as_blob(not_long_wide, false), as_blob(*rcw1));
  ASSERT_EQ(0xABBA, control.control_key_state());
  ASSERT_EQ(0xBCCB, backend.last_control.ctrl_wakeup_mask());
  ASSERT_EQ(10, backend.last_control.initial_chars());
}

// A backend that fails on *everything*. Don't forget to add a test when you
// add a new method.
class FailingConsoleBackend : public ConsoleBackend {
public:
  FailingConsoleBackend(uint32_t a = 1, uint32_t b = 1) : a_(a), b_(b) { }
  response_t<bool_t> connect(Handle stdin_handle, Handle stdout_handle,
      Handle stderr_handle) { return response_t<bool_t>::yes(); }
  response_t<int64_t> poke(int64_t value) { return fail<int64_t>(); }
  response_t<uint32_t> get_console_cp(bool is_output) { return fail<uint32_t>(); }
  response_t<bool_t> set_console_cp(uint32_t value, bool is_output) { return fail<bool_t>(); }
  response_t<uint32_t> get_console_title(tclib::Blob buffer, bool is_unicode, size_t *bytes_written_out) { return fail<uint32_t>(); }
  response_t<bool_t> set_console_title(tclib::Blob title, bool is_unicode) { return fail<bool_t>(); }
  response_t<bool_t> set_console_mode(Handle handle, uint32_t mode) { return fail<bool_t>(); }
  response_t<bool_t> get_console_screen_buffer_info(Handle buffer, ScreenBufferInfo *info_out) { return fail<bool_t>(); }
  response_t<uint32_t> write_console(Handle output, tclib::Blob data, bool is_unicode) { return fail<uint32_t>(); }
  response_t<uint32_t> read_console(Handle output, tclib::Blob buffer, bool is_unicode, size_t *bytes_read, ReadConsoleControl *input_control) { return fail<uint32_t>(); }

  // Returns the next error code in the sequence.
  uint32_t gen_error();

private:
  template <typename T>
  response_t<T> fail() { return response_t<T>::error(gen_error()); }
  uint32_t a_;
  uint32_t b_;
};

uint32_t FailingConsoleBackend::gen_error() {
  // This used to be the Fibonacci sequence but it grew too fast since we need
  // the values to fit in 16 bits. This grows more slowly.
  uint32_t old_a = a_;
  a_ = b_;
  b_ = old_a + (a_ / 2);
  return old_a;
}

static int64_t get_last_error(const DriverRequest &request) {
  // Casting away the const here is super cheating but it would be sooo tedious
  // to create a whole set of parallel const methods to be able to do this
  // const.
  Variant error = const_cast<DriverRequest&>(request).error();
  return error.native_as<ConsoleError>()->code();
}

AGENT_TEST(failures) {
  FailingConsoleBackend backend(3, 4);
  AGENT_TEST_PREAMBLE(&backend, use_real);

  // In the simulated version the values from the backend. In the real version
  // it's the negated arguments. In both cases the values are the same.
  ASSERT_EQ(3, get_last_error(driver.poke_backend(-3)));
  ASSERT_EQ(4, get_last_error(driver.poke_backend(-4)));

  if (use_real) {
    // If we're using the real console there's no backend so we need to advance
    // it manually for the codes to match up.
    ASSERT_EQ(3, backend.gen_error());
    ASSERT_EQ(4, backend.gen_error());
  }

  ASSERT_EQ(5, get_last_error(driver.get_console_cp()));
  ASSERT_EQ(6, get_last_error(driver.get_console_output_cp()));

  ASSERT_EQ(8, get_last_error(driver.set_console_cp(10)));
  ASSERT_EQ(10, get_last_error(driver.set_console_output_cp(10)));

  ASSERT_EQ(13, get_last_error(driver.get_console_title_a(1)));
  ASSERT_EQ(16, get_last_error(driver.set_console_title_a("foo")));
  Handle dummy_handle(10);
  ASSERT_EQ(21, backend.gen_error()); // Test removed but still pull an error.
  ASSERT_EQ(26, get_last_error(driver.set_console_mode(dummy_handle, 0xFF)));

  ASSERT_EQ(34, get_last_error(driver.get_console_screen_buffer_info(dummy_handle)));

  const wide_char_t wide_foo[] = {'f', 'o', 'o', 0};
  ASSERT_EQ(43, get_last_error(driver.get_console_title_w(1)));
  ASSERT_EQ(55, get_last_error(driver.set_console_title_w(wide_foo)));

  ASSERT_EQ(70, get_last_error(driver.write_console_a(dummy_handle, StringUtils::as_blob("Foo"))));
  ASSERT_EQ(90, get_last_error(driver.write_console_w(dummy_handle, StringUtils::as_blob(wide_foo))));
  ASSERT_EQ(115, get_last_error(driver.read_console_a(dummy_handle, 10)));
  ASSERT_EQ(147, get_last_error(driver.read_console_w(dummy_handle, 10)));
}
