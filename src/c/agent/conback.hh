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

class ConsoleAgent;

// Abstract console backend type.
class ConsoleBackend : public tclib::DefaultDestructable {
public:
  virtual ~ConsoleBackend() { }

  // Send a debug/test poke to this backend.
  virtual int64_t poke(int64_t value) = 0;
};

// Concrete console backend that that is backed by a remote agent.
class RemoteConsoleBackend : public ConsoleBackend {
public:
  RemoteConsoleBackend(ConsoleAgent *agent) : agent_(agent) { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  virtual int64_t poke(int64_t value);

  static tclib::pass_def_ref_t<ConsoleBackend> create(ConsoleAgent *agent);

private:
  ConsoleAgent *agent_;
  ConsoleAgent *agent() { return agent_; }
};

} // conprx

#endif // _AGENT_CONSOLE_BACKEND_HH
