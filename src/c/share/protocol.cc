//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "share/protocol.hh"

#include "marshal-inl.hh"
#include "utils/callback.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/misc-inl.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

ScreenBufferInfo::ScreenBufferInfo() {
  struct_zero_fill(raw_);
  raw_.cbSize = sizeof(raw_);
}

ReadConsoleControl::ReadConsoleControl() {
  struct_zero_fill(raw_);
  raw_.nLength = sizeof(raw_);
}

TypeRegistry *ConsoleTypes::registry() {
  static TypeRegistry *instance = NULL;
  if (instance == NULL) {
    instance = new TypeRegistry();
    instance->register_type<Handle>();
    instance->register_type<coord_t>();
    instance->register_type<small_rect_t>();
    instance->register_type<console_screen_buffer_info_t>();
    instance->register_type<console_screen_buffer_infoex_t>();
    instance->register_type<console_readconsole_control_t>();
    instance->register_type<LogEntry>();
    instance->register_type<NativeProcessInfo>();
  }
  return instance;
}

DefaultSeedType<Handle> Handle::kSeedType("winapi.HANDLE");

Handle *Handle::new_instance(Variant header, Factory *factory) {
  return new (factory) Handle();
}

Variant Handle::to_seed(Factory *factory) {
  Seed result = factory->new_seed(seed_type());
  result.set_field("id", id_);
  return result;
}

void Handle::init(Seed payload, Factory *factory) {
  id_ = payload.get_field("id").integer_value(kInvalidHandleValue);
}

DefaultSeedType<NativeProcessInfo> NativeProcessInfo::kSeedType("conprx.NativeProcessInfo");

NativeProcessInfo *NativeProcessInfo::new_instance(Variant header, Factory *factory) {
  return new (factory) NativeProcessInfo(0);
}

Variant NativeProcessInfo::to_seed(plankton::Factory *factory) {
  Seed result = factory->new_seed(seed_type());
  result.set_field("id", raw_id());
  return result;
}

void NativeProcessInfo::init(plankton::Seed payload, plankton::Factory *factory) {
  raw_id_ = payload.get_field("id").integer_value(0);
}

NtStatus NtStatus::from(Severity severity, Provider provider, Facility facility,
    uint32_t code) {
  return severity | provider | facility | (code & kCodeMask);
}

static console_screen_buffer_info_t *new_console_screen_buffer_info(Variant header, Factory *factory) {
  console_screen_buffer_info_t *result = new (factory) console_screen_buffer_info_t;
  struct_zero_fill(*result);
  return result;
}

static void init_console_screen_buffer_info(console_screen_buffer_info_t *info,
    Seed payload, Factory *factory) {
  coord_t zero_coord = {0, 0};
  info->dwSize = *payload.get_field("dwSize").native_as_or_else<coord_t>(&zero_coord);
  info->dwCursorPosition = *payload.get_field("dwCursorPosition").native_as_or_else<coord_t>(&zero_coord);
  info->wAttributes = static_cast<uint16_t>(payload.get_field("wAttributes").integer_value());
  small_rect_t zero_rect = {0, 0, 0, 0};
  info->srWindow = *payload.get_field("srWindow").native_as_or_else<small_rect_t>(&zero_rect);
  info->dwMaximumWindowSize = *payload.get_field("dwMaximumWindowSize").native_as_or_else<coord_t>(&zero_coord);
}

static Variant console_screen_buffer_info_to_seed(console_screen_buffer_info_t *info,
    Factory *factory) {
  Seed seed = factory->new_seed(default_seed_type<console_screen_buffer_info_t>::get());
  seed.set_field("dwSize", factory->new_native(&info->dwSize));
  seed.set_field("dwCursorPosition", factory->new_native(&info->dwCursorPosition));
  seed.set_field("wAttributes", info->wAttributes);
  seed.set_field("srWindow", factory->new_native(&info->srWindow));
  seed.set_field("dwMaximumWindowSize", factory->new_native(&info->dwMaximumWindowSize));
  return seed;
}

static plankton::SeedType<console_screen_buffer_info_t> console_info_seed_type(
    "winapi.CONSOLE_SCREEN_BUFFER_INFO", &new_console_screen_buffer_info,
    &init_console_screen_buffer_info, &console_screen_buffer_info_to_seed);

ConcreteSeedType<console_screen_buffer_info_t> *default_seed_type<console_screen_buffer_info_t>::get() {
  return &console_info_seed_type;
}

static console_screen_buffer_infoex_t *new_console_screen_buffer_infoex(Variant header, Factory *factory) {
  console_screen_buffer_infoex_t *result = new (factory) console_screen_buffer_infoex_t;
  struct_zero_fill(*result);
  return result;
}

static void init_console_screen_buffer_infoex(console_screen_buffer_infoex_t *info,
    Seed payload, Factory *factory) {
  init_console_screen_buffer_info(console_screen_buffer_info_from_ex(info),
      payload, factory);
  info->wPopupAttributes = static_cast<uint16_t>(payload.get_field("wPopupAttributes").integer_value());
  info->bFullscreenSupported = payload.get_field("bFullscreenSupported").bool_value();
  Array color_table = payload.get_field("ColorTable");
  for (uint32_t i = 0; i < min_size(16, color_table.length()); i++)
    info->ColorTable[i] = static_cast<colorref_t>(color_table[i].integer_value());
}

static Variant console_screen_buffer_infoex_to_seed(console_screen_buffer_infoex_t *info,
    Factory *factory) {
  Seed seed = factory->new_seed(default_seed_type<console_screen_buffer_infoex_t>::get());
  seed.set_field("dwSize", factory->new_native(&info->dwSize));
  seed.set_field("dwCursorPosition", factory->new_native(&info->dwCursorPosition));
  seed.set_field("wAttributes", info->wAttributes);
  seed.set_field("srWindow", factory->new_native(&info->srWindow));
  seed.set_field("dwMaximumWindowSize", factory->new_native(&info->dwMaximumWindowSize));
  seed.set_field("wPopupAttributes", info->wPopupAttributes);
  seed.set_field("bFullscreenSupported", Variant::boolean(info->bFullscreenSupported));
  Array color_table = factory->new_array(16);
  for (size_t i = 0; i < 16; i++)
    color_table.add(info->ColorTable[i]);
  seed.set_field("ColorTable", color_table);
  return seed;
}

static plankton::SeedType<console_screen_buffer_infoex_t> console_infoex_seed_type(
    "winapi.CONSOLE_SCREEN_BUFFER_INFOEX", &new_console_screen_buffer_infoex,
    &init_console_screen_buffer_infoex, &console_screen_buffer_infoex_to_seed);

ConcreteSeedType<console_screen_buffer_infoex_t> *default_seed_type<console_screen_buffer_infoex_t>::get() {
  return &console_infoex_seed_type;
}

static coord_t *new_coord(Variant header, Factory *factory) {
  coord_t *result = new (factory) coord_t;
  *result = coord_new(0, 0);
  return result;
}

static void init_coord(coord_t *coord, Seed payload, Factory *factory) {
  coord->X = static_cast<int16_t>(payload.get_field("X").integer_value());
  coord->Y = static_cast<int16_t>(payload.get_field("Y").integer_value());
}

static Variant coord_to_seed(coord_t *coord, Factory *factory) {
  Seed seed = factory->new_seed(default_seed_type<coord_t>::get());
  seed.set_field("X", coord->X);
  seed.set_field("Y", coord->Y);
  return seed;
}

static plankton::SeedType<coord_t> coord_seed_type("winapi.COORD", &new_coord,
    &init_coord, &coord_to_seed);

ConcreteSeedType<coord_t> *default_seed_type<coord_t>::get() {
  return &coord_seed_type;
}

static small_rect_t *new_small_rect(Variant header, Factory *factory) {
  small_rect_t *result = new (factory) small_rect_t;
  *result = small_rect_new(0, 0, 0, 0);
  return result;
}

static void init_small_rect(small_rect_t *rect, Seed payload, Factory *factory) {
  rect->Left = static_cast<int16_t>(payload.get_field("Left").integer_value());
  rect->Top = static_cast<int16_t>(payload.get_field("Top").integer_value());
  rect->Right = static_cast<int16_t>(payload.get_field("Right").integer_value());
  rect->Bottom = static_cast<int16_t>(payload.get_field("Bottom").integer_value());
}

static Variant small_rect_to_seed(small_rect_t *rect, Factory *factory) {
  Seed seed = factory->new_seed(default_seed_type<small_rect_t>::get());
  seed.set_field("Left", rect->Left);
  seed.set_field("Top", rect->Top);
  seed.set_field("Right", rect->Right);
  seed.set_field("Bottom", rect->Bottom);
  return seed;
}

static plankton::SeedType<small_rect_t> small_rect_seed_type("winapi.SMALL_RECT",
    &new_small_rect, &init_small_rect, &small_rect_to_seed);

ConcreteSeedType<small_rect_t> *default_seed_type<small_rect_t>::get() {
  return &small_rect_seed_type;
}

static console_readconsole_control_t *new_readcontrol(Variant header, Factory *factory) {
  ReadConsoleControl *result = new (factory) ReadConsoleControl();
  return result->raw();
}

static void init_readcontrol(console_readconsole_control_t *control, Seed payload, Factory *factory) {
  control->nInitialChars = static_cast<ulong_t>(payload.get_field("nInitialChars").integer_value());
  control->dwCtrlWakeupMask = static_cast<ulong_t>(payload.get_field("dwCtrlWakeupMask").integer_value());
  control->dwControlKeyState = static_cast<ulong_t>(payload.get_field("dwControlKeyState").integer_value());
}

static Variant readcontrol_to_seed(console_readconsole_control_t *control, Factory *factory) {
  Seed seed = factory->new_seed(default_seed_type<console_readconsole_control_t>::get());
  seed.set_field("nInitialChars", control->nInitialChars);
  seed.set_field("dwCtrlWakeupMask", control->dwCtrlWakeupMask);
  seed.set_field("dwControlKeyState", control->dwControlKeyState);
  return seed;
}

static plankton::SeedType<console_readconsole_control_t> readcontrol_seed_type(
    "winapi.CONSOLE_READCONSOLE_CONTROL",
    &new_readcontrol, &init_readcontrol, &readcontrol_to_seed);

ConcreteSeedType<console_readconsole_control_t> *default_seed_type<console_readconsole_control_t>::get() {
  return &readcontrol_seed_type;
}
