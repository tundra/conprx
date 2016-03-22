//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver.hh"
#include "share/protocol.hh"

#include "marshal-inl.hh"
#include "utils/callback.hh"

using namespace conprx;
using namespace plankton;
using namespace tclib;

TypeRegistry *ConsoleProxy::registry() {
  static TypeRegistry *instance = NULL;
  if (instance == NULL) {
    instance = new TypeRegistry();
    instance->register_type<Handle>();
    instance->register_type<ConsoleError>();
  }
  return instance;
}

DefaultSeedType<ConsoleError> ConsoleError::kSeedType("conprx.ConsoleError");

ConsoleError *ConsoleError::new_instance(Variant header, Factory *factory) {
  return new (factory) ConsoleError();
}

Variant ConsoleError::to_seed(Factory *factory) {
  Seed result = factory->new_seed(seed_type());
  result.set_field("last_error", last_error_.to_nt());
  return result;
}

void ConsoleError::init(Seed payload, Factory *factory) {
  Variant last_error = payload.get_field("last_error");
  last_error_ = last_error.is_integer()
      ? NtStatus::from_nt(static_cast<uint32_t>(last_error.integer_value()))
      : NtStatus::success();
}
