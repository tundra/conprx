//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// A console connector connects the implementation of the console api to an
/// implementation. The implementation in kernel32 turns console function calls
/// into remote messages through windows' LPC system and a console connector is
/// a replacement for those remote calls.

#ifndef _AGENT_CONSOLE_CONNECTOR_HH
#define _AGENT_CONSOLE_CONNECTOR_HH

#include "conapi-types.hh"
#include "io/stream.hh"
#include "plankton-inl.hh"
#include "rpc.hh"
#include "agent/response.hh"

namespace conprx {

// Abstract console backend type.
class ConsoleConnector : public tclib::DefaultDestructable {
public:
  virtual ~ConsoleConnector() { }

  // Send a debug/test poke to this backend.
  virtual Response<int64_t> poke(int64_t value) = 0;

  virtual Response<int64_t> get_console_cp() = 0;
};

// Concrete console connector that is implemented by sending messages over
// plankton rpc.
class PrpcConsoleConnector : public ConsoleConnector {
public:
  PrpcConsoleConnector(plankton::rpc::MessageSocket *socket, plankton::InputSocket *in);
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  virtual Response<int64_t> poke(int64_t value);
  virtual Response<int64_t> get_console_cp();

  static tclib::pass_def_ref_t<ConsoleConnector> create(
      plankton::rpc::MessageSocket *socket, plankton::InputSocket *in);

private:
  // Send a request through the socket and wait for a response, converting it
  // to an API response of the given type using the given converter.
  template <typename T, typename C>
  Response<T> send_request(plankton::rpc::OutgoingRequest *request,
      plankton::rpc::IncomingResponse *response_out);

  // Works the same way as send_request but uses the default converter for the
  // type T instead of requiring one to be specified.
  template <typename T>
  Response<T> send_request_default(plankton::rpc::OutgoingRequest *request,
      plankton::rpc::IncomingResponse *response_out);

  plankton::rpc::MessageSocket *socket_;
  plankton::rpc::MessageSocket *socket() { return socket_; }
  plankton::InputSocket *in_;
  plankton::InputSocket *in() { return in_; }
};

} // conprx

#endif // _AGENT_CONSOLE_CONNECTOR_HH