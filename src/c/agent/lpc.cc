//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/lpc.hh"

using namespace lpc;
using namespace tclib;

Interceptor *Interceptor::current_ = NULL;

Interceptor::Disable::Disable(Interceptor* interceptor)
  : interceptor_(interceptor)
  , was_enabled_(interceptor->enabled_) {
  interceptor->enabled_ = false;
}

Interceptor::Disable::~Disable() {
  interceptor_->enabled_ = was_enabled_;
}

Message::Message(message_data_t *data, AddressXform *xform)
  : data_(data)
  , capbuf_(data->capture_buffer, xform, data) {
}

tclib::Blob CaptureBuffer::block(size_t index) {
  return tclib::Blob();
}

#ifdef IS_MSVC
#  include "lpc-msvc.cc"
#endif
