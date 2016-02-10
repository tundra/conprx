//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver.hh"

#include "marshal-inl.hh"
#include "utils/callback.hh"

using namespace conprx;
using namespace plankton;
using namespace tclib;

DefaultSeedType<Handle> Handle::kSeedType("conprx.Handle");

Handle *Handle::new_instance(Variant header, Factory *factory) {
  return new (factory) Handle();
}

Variant Handle::to_seed(Factory *factory) {
  Seed result = factory->new_seed(seed_type());
  result.set_field("id", id_);
  return result;
}

void Handle::init(Seed payload, Factory *factory) {
  Variant id = payload.get_field("id");
  id_ = id.is_integer() ? id.integer_value() : kInvalidHandleValue;
}

TypeRegistry *ConsoleProxy::registry() {
  static TypeRegistry *instance = NULL;
  if (instance == NULL) {
    instance = new TypeRegistry();
    instance->register_type(Handle::seed_type());
    instance->register_type(ConsoleError::seed_type());
  }
  return instance;
}

DefaultSeedType<ConsoleError> ConsoleError::kSeedType("conprx.ConsoleError");

ConsoleError *ConsoleError::new_instance(Variant header, Factory *factory) {
  return new (factory) ConsoleError();
}

Variant ConsoleError::to_seed(Factory *factory) {
  Seed result = factory->new_seed(seed_type());
  result.set_field("last_error", last_error_);
  return result;
}

void ConsoleError::init(Seed payload, Factory *factory) {
  Variant last_error = payload.get_field("last_error");
  last_error_ = last_error.is_integer() ? last_error.integer_value() : -1;
}
