//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// A console connector connects the implementation of the console api to an
/// implementation. The implementation in kernel32 turns console function calls
/// into remote messages through windows' LPC system and a console connector is
/// a replacement for those remote calls.

#ifndef _AGENT_CONSOLE_CONNECTOR_HH
#define _AGENT_CONSOLE_CONNECTOR_HH

#include "agent/lpc.hh"
#include "conapi-types.hh"
#include "io/stream.hh"
#include "plankton-inl.hh"
#include "rpc.hh"

namespace conprx {

// Abstract console backend type.
class ConsoleConnector : public tclib::DefaultDestructable {
public:
  virtual ~ConsoleConnector() { }

  // Send a debug/test poke to this backend.
  virtual response_t<int64_t> poke(int64_t value) = 0;

  // Return one of the console code pages, input or output.
  virtual response_t<uint32_t> get_console_cp(bool is_output) = 0;

  // Set one of the console code pages, input or output.
  virtual response_t<bool_t> set_console_cp(uint32_t value, bool is_output) = 0;

  // Set the console title. The blob contains the raw character data, ascii if
  // is_unicode is false, wide if true. The blob is not guaranteed to contain a
  // null terminator so don't assume it does.
  virtual response_t<bool_t> set_console_title(tclib::Blob title, bool is_unicode) = 0;

  // Read the title of the console and store it in the given buffer.
  virtual response_t<uint32_t> get_console_title(tclib::Blob buffer, bool is_unicode) = 0;

  virtual response_t<bool_t> set_console_mode(void *handle, uint32_t mode) = 0;

  virtual response_t<uint32_t> get_console_mode(handle_t handle) = 0;

  virtual response_t<bool_t> get_console_screen_buffer_info(Handle buffer,
      console_screen_buffer_info_t *info_out) = 0;
};

// A console adaptor converts raw lpc messages into plankton messages to send
// through a connector.
class ConsoleAdaptor : public tclib::DefaultDestructable {
public:
  ConsoleAdaptor(ConsoleConnector *connector) : connector_(connector) { }
  virtual ~ConsoleAdaptor() { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

  // Generate the handler declarations.
#define __GEN_HANDLER__(Name, name, DLL, API)                                  \
  NtStatus name(lpc::Message *req, lpc::name##_m *data);
  FOR_EACH_LPC_TO_INTERCEPT(__GEN_HANDLER__)
#undef __GEN_HANDLER__

  response_t<int64_t> poke(int64_t value);

private:
  ConsoleConnector *connector() { return connector_; }
  ConsoleConnector *connector_;
};

// Concrete console connector that is implemented by sending messages over
// plankton rpc.
class PrpcConsoleConnector : public ConsoleConnector {
public:
  PrpcConsoleConnector(plankton::rpc::MessageSocket *socket, plankton::InputSocket *in);
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  virtual response_t<int64_t> poke(int64_t value);
  virtual response_t<uint32_t> get_console_cp(bool is_output);
  virtual response_t<bool_t> set_console_cp(uint32_t value, bool is_output);
  virtual response_t<bool_t> set_console_title(tclib::Blob data, bool is_unicode);
  virtual response_t<uint32_t> get_console_title(tclib::Blob buffer, bool is_unicode);
  virtual response_t<bool_t> set_console_mode(handle_t handle, uint32_t mode);
  virtual response_t<uint32_t> get_console_mode(handle_t handle);
  virtual response_t<bool_t> get_console_screen_buffer_info(Handle buffer,
      console_screen_buffer_info_t *info_out);

  static tclib::pass_def_ref_t<ConsoleConnector> create(
      plankton::rpc::MessageSocket *socket, plankton::InputSocket *in);

private:
  // Send a request through the socket and wait for a response, converting it
  // to an API response of the given type using the given converter.
  template <typename T, typename C>
  response_t<T> send_request(plankton::rpc::OutgoingRequest *request,
      plankton::rpc::IncomingResponse *response_out);

  // Works the same way as send_request but uses the default converter for the
  // type T instead of requiring one to be specified.
  template <typename T>
  response_t<T> send_request_default(plankton::rpc::OutgoingRequest *request,
      plankton::rpc::IncomingResponse *response_out);

  plankton::rpc::MessageSocket *socket_;
  plankton::rpc::MessageSocket *socket() { return socket_; }
  plankton::InputSocket *in_;
  plankton::InputSocket *in() { return in_; }
};

} // conprx

#endif // _AGENT_CONSOLE_CONNECTOR_HH
