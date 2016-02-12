//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// A console backend is what implements the console. The distinction between
/// front- and backend comes from the fact that we're generally not patching
/// the console api itself but the calls the api's implementation makes to the
/// console server. So the backend is a replacement for that server and its
/// api is very much dictated by what the windows console api implementation
/// requires from that layer.

#ifndef _AGENT_CONSOLE_BACKEND_HH
#define _AGENT_CONSOLE_BACKEND_HH

#include "conapi-types.hh"
#include "io/stream.hh"
#include "plankton-inl.hh"
#include "rpc.hh"

namespace conprx {

// Abstract console backend type.
class ConsoleBackend : public tclib::DefaultDestructable {
public:
  virtual ~ConsoleBackend() { }

  virtual bool set_title(void *title, dword_t length, bool is_unicode) = 0;

  virtual void handle_call() = 0;
};

// Concrete console backend that that is backed by a remote service over
// plankton rpc
class RemoteConsoleBackend : public ConsoleBackend {
public:
  RemoteConsoleBackend(tclib::InStream *in, tclib::OutStream *out);

public:
  plankton::rpc::StreamServiceConnector connector_;
};

} // conprx

#endif // _AGENT_CONSOLE_BACKEND_HH
