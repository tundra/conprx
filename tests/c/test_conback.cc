//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/agent.hh"
#include "agent/conconn.hh"
#include "agent/confront.hh"
#include "bytestream.hh"
#include "rpc.hh"
#include "server/conback.hh"
#include "test.hh"
#include "utils/string.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;
using namespace conprx;

class SimulatedAgent : public ConsoleAgent {
public:
  SimulatedAgent(ConsoleConnector *connector) : adaptor_(connector) { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  virtual fat_bool_t install_agent_platform() { return F_TRUE; }
  virtual fat_bool_t uninstall_agent_platform() { return F_TRUE; }
  virtual ConsoleAdaptor *adaptor() { return &adaptor_; }

private:
  ConsoleAdaptor adaptor_;
};

// An in-memory adaptor that provides a front-end backed by simulating requests
// to the given backend. The reason for testing backends by calling a frontend
// instead of calls to the backend directly is that it allows the tests to be
// run both against the simulated frontend but also the actual windows console,
// that way ensuring that they behave the same way which is what we're really
// interested in.
class SimulatedFrontendAdaptor : public tclib::DefaultDestructable {
public:
  SimulatedFrontendAdaptor(ConsoleBackend *backend);
  void set_trace(bool value) { trace_ = value; }
  virtual ~SimulatedFrontendAdaptor() { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  fat_bool_t initialize();
  ConsoleFrontend *operator->() { return frontend(); }
  ConsoleFrontend *frontend() { return *frontend_; }
private:
  ConsoleBackend *backend_;
  ByteBufferStream buffer_;
  rpc::StreamServiceConnector streams_;
  PrpcConsoleConnector connector_;
  SimulatedAgent agent_;
  def_ref_t<ConsoleFrontend> frontend_;
  ConsoleBackendService service_;
  bool trace_;
  rpc::TracingMessageSocketObserver tracer_;
};

SimulatedFrontendAdaptor::SimulatedFrontendAdaptor(ConsoleBackend *backend)
  : backend_(backend)
  , buffer_(1024)
  , streams_(&buffer_, &buffer_)
  , connector_(streams_.socket(), streams_.input())
  , agent_(&connector_)
  , trace_(false)
  , tracer_("SB") {
  frontend_ = ConsoleFrontend::new_simulating(&agent_, 0);
  service_.set_backend(backend_);
}

fat_bool_t SimulatedFrontendAdaptor::initialize() {
  if (trace_)
    tracer_.install(streams_.socket());
  if (!buffer_.initialize())
    return F_FALSE;
  F_TRY(streams_.init(service_.handler()));
  return F_TRUE;
}

// Records all relevant state about a frontend (within reason) by querying the
// frontend and can restore the state.
class FrontendSnapshot {
public:
  FrontendSnapshot();

  // Records the state of the given frontend into this snapshot.
  void save(ConsoleFrontend *frontend);

  // Updates the state of the given frontend to match this snapshot.
  void restore(ConsoleFrontend *frontend);

  // Print the state of this snapshot on the given stream.
  void dump(OutStream *out);

private:
  uint32_t input_codepage_;
  uint32_t output_codepage_;
  char title_[1024];
};

FrontendSnapshot::FrontendSnapshot()
  : input_codepage_(0)
  , output_codepage_(0) {
  struct_zero_fill(title_);
}

void FrontendSnapshot::save(ConsoleFrontend *frontend) {
  input_codepage_ = frontend->get_console_cp();
  output_codepage_ = frontend->get_console_output_cp();
  frontend->get_console_title_a(title_, 1024);
}

void FrontendSnapshot::restore(ConsoleFrontend *frontend) {
  frontend->set_console_cp(input_codepage_);
  frontend->set_console_output_cp(output_codepage_);
  frontend->set_console_title_a(title_);
}

void FrontendSnapshot::dump(OutStream *out) {
  out->printf("input_codepage: %i\n", input_codepage_);
  out->printf("output_codepage: %i\n", output_codepage_);
  out->printf("title: [%s]\n", title_);
}

// A class that holds a frontend, either a native one (only on windows) or a
// simulated one on top of a basic backend. This also takes care of saving
// and restoring the state so mutating the native console doesn't mess up the
// window the test is running in.
class FrontendMultiplexer {
public:
  FrontendMultiplexer(bool use_native)
    : use_native_(use_native)
    , frontend_(NULL)
    , trace_(false) { }
  ~FrontendMultiplexer();
  ConsoleFrontend *operator->() { return frontend_; }
  fat_bool_t initialize();
  void set_trace(bool value) { trace_ = value; }

private:
  bool use_native_;
  ConsoleFrontend *frontend_;
  BasicConsoleBackend backend_;
  def_ref_t<SimulatedFrontendAdaptor> fake_;
  def_ref_t<ConsoleFrontend> native_;
  FrontendSnapshot initial_state_;
  bool trace_;
};

fat_bool_t FrontendMultiplexer::initialize() {
  if (use_native_) {
    native_ = IF_MSVC(ConsoleFrontend::new_native(), pass_def_ref_t<ConsoleFrontend>::null());
    frontend_ = *native_;
  } else {
    fake_ = new (kDefaultAlloc) SimulatedFrontendAdaptor(&backend_);
    fake_->set_trace(trace_);
    F_TRY(fake_->initialize());
    frontend_ = fake_->frontend();
  }
  initial_state_.save(frontend_);
  return F_TRUE;
}

FrontendMultiplexer::~FrontendMultiplexer() {
  initial_state_.restore(frontend_);
}

TEST(conback, simulated) {
  BasicConsoleBackend backend;
  SimulatedFrontendAdaptor frontend(&backend);
  ASSERT_TRUE(frontend.initialize());
  ASSERT_EQ(0, frontend->poke_backend(423));
  ASSERT_EQ(423, frontend->poke_backend(653));
  ASSERT_EQ(653, frontend->poke_backend(374));
  ASSERT_EQ(374, backend.last_poke());
}

// Declare a console backend test that runs both the same tests against the
// native and basic backend.
#define CONBACK_TEST(NAME) MULTITEST(conback, NAME, bool_t, use_native, ("native", true), ("basic", false))

#define SKIP_IF_UNSUPPORTED() do {                                             \
  if (use_native && !kIsMsvc)                                                  \
    SKIP_TEST("msvc only");                                                    \
} while (false)

CONBACK_TEST(cp) {
  SKIP_IF_UNSUPPORTED();
  FrontendMultiplexer frontend(use_native);
  ASSERT_F_TRUE(frontend.initialize());

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

CONBACK_TEST(title_a) {
  SKIP_IF_UNSUPPORTED();
  FrontendMultiplexer frontend(use_native);
  ASSERT_F_TRUE(frontend.initialize());

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

CONBACK_TEST(title_w) {
  SKIP_IF_UNSUPPORTED();
  FrontendMultiplexer frontend(use_native);
  ASSERT_F_TRUE(frontend.initialize());

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
      StringUtils::as_blob(buf, false));
  ASSERT_EQ(wide_minus_1, buf[17]);

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_w(buf, 50));
  ASSERT_BLOBEQ(tclib::Blob(letters, 24 * sizeof(wide_char_t)),
      StringUtils::as_blob(buf, false));
  ASSERT_EQ(wide_minus_1, buf[26]);

  bufblob.fill(-1);
  ASSERT_EQ(26, frontend->get_console_title_w(buf, 52));
  ASSERT_BLOBEQ(tclib::Blob(letters, 25 * sizeof(wide_char_t)),
      StringUtils::as_blob(buf, false));
  ASSERT_EQ(wide_minus_1, buf[27]);
}

CONBACK_TEST(title_aw_ascii) {
  SKIP_IF_UNSUPPORTED();
  FrontendMultiplexer frontend(use_native);
  ASSERT_F_TRUE(frontend.initialize());

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

CONBACK_TEST(title_aw_unicode) {
  SKIP_TEST("waiting for CP437 codec");
  SKIP_IF_UNSUPPORTED();
  FrontendMultiplexer frontend(use_native);
  ASSERT_F_TRUE(frontend.initialize());

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
}
