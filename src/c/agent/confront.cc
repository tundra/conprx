//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Shared console api implementation.

#include "confront.hh"

#include "utils/string.hh"
#include "plankton-inl.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;

ConsoleFrontend::~ConsoleFrontend() { }

static ConsoleFrontend::FunctionInfo function_info[ConsoleFrontend::kFunctionCount + 1] = {
#define __DECLARE_INFO__(Name, name, ...) {TEXT(#Name), ConsoleFrontend::name##_key} ,
    FOR_EACH_CONAPI_FUNCTION(__DECLARE_INFO__)
#undef __DECLARE_INFO__
    {NULL, 0}
};

Vector<ConsoleFrontend::FunctionInfo> ConsoleFrontend::functions() {
  return Vector<FunctionInfo>(function_info, ConsoleFrontend::kFunctionCount);
}

// --- L o g g i n g ---

// A plankton arena with some extra utilities for console api logging.
class log_arena_t : public Arena {
public:
  // Allocate a new arena string with the same contents as the given utf16
  // string.
  Variant new_utf16(const void *raw_utf16, size_t length);

  // Returns a coordinate variant corresponding to the given value.
  Variant new_coord(const coord_t &coord);

  // Returns a small rect variant corresponding to the given value.
  Variant new_small_rect(const small_rect_t &small_rect);
};

Variant log_arena_t::new_utf16(const void *raw_utf16, size_t utf16_length) {
  wide_cstr_t utf16 = static_cast<wide_cstr_t>(raw_utf16);
  char *utf8 = NULL;
  size_t utf8_length = conprx::String::utf16_to_utf8(utf16, utf16_length, &utf8);
  Variant result = new_string(utf8, (uint32_t) utf8_length);
  delete[] utf8;
  return result;
}

Variant log_arena_t::new_coord(const coord_t &coord) {
  Map map = new_map();
  map.set("x", coord.X);
  map.set("y", coord.Y);
  return map;
}

Variant log_arena_t::new_small_rect(const small_rect_t &small_rect) {
  Map map = new_map();
  map.set("left", small_rect.Left);
  map.set("top", small_rect.Top);
  map.set("right", small_rect.Right);
  map.set("bottom", small_rect.Bottom);
  return map;
}

void LoggingConsole::emit_message(Variant message) {
  TextWriter writer;
  writer.write(message);
  INFO("\n%s\n", *writer);
}

static Variant handle_variant(handle_t handle) {
  return Variant::integer(reinterpret_cast<int64_t>(handle));
}

// Macro that sets up the variables used to produce a log message. After this
// the value 'arena' and 'message' are visible. The order of message fields
// should be:
//
//   - Input fields that the caller is required to give a meaningful value.
//   - The return value of the call.
//   - If the call succeeded any returned values the called function is expected
//     to have set. If the call fails these should be left out.
#define BEGIN_LOG_MESSAGE(Op) {                                                \
  log_arena_t arena;                                                           \
  Map message = arena.new_map();                                               \
  message.set("op", Op);                                                       \
  do { } while (false)

// Completes a log message, sending it and disposing of the data structures.
#define END_LOG_MESSAGE()                                                      \
  emit_message(message);                                                       \
}                                                                              \
do { } while (false)

bool_t LoggingConsole::alloc_console() {
  bool_t result = delegate().alloc_console();
  BEGIN_LOG_MESSAGE("AllocConsole");
    message.set("result", Variant::boolean(result));
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::free_console() {
  bool_t result = delegate().free_console();
  BEGIN_LOG_MESSAGE("FreeConsole");
    message.set("result", Variant::boolean(result));
  END_LOG_MESSAGE();
  return result;
}

uint_t LoggingConsole::get_console_cp() {
  uint_t result = delegate().get_console_cp();
  BEGIN_LOG_MESSAGE("GetConsoleCP");
    message.set("result", result);
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::get_console_cursor_info(handle_t console_output,
    console_cursor_info_t *console_cursor_info) {
  bool_t result = delegate().get_console_cursor_info(console_output,
      console_cursor_info);
  BEGIN_LOG_MESSAGE("GetConsoleCursorInfo");
    message.set("console_output", handle_variant(console_output));
    message.set("result", Variant::boolean(result));
    if (result) {
      message.set("size", console_cursor_info->dwSize);
      message.set("visible", Variant::boolean(console_cursor_info->bVisible));
    }
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::get_console_mode(handle_t console_handle, dword_t *mode) {
  bool_t result = delegate().get_console_mode(console_handle, mode);
  BEGIN_LOG_MESSAGE("GetConsoleMode");
    message.set("console_handle", handle_variant(console_handle));
    message.set("result", Variant::boolean(result));
    if (result)
      message.set("mode", *mode);
  END_LOG_MESSAGE();
  return result;
}

uint_t LoggingConsole::get_console_output_cp() {
  uint_t result = delegate().get_console_output_cp();
  BEGIN_LOG_MESSAGE("GetConsoleOutputCP");
    message.set("result", result);
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::get_console_screen_buffer_info(handle_t console_output,
    console_screen_buffer_info_t *console_screen_buffer_info) {
  bool_t result = delegate().get_console_screen_buffer_info(console_output,
      console_screen_buffer_info);
  BEGIN_LOG_MESSAGE("GetConsoleScreenBufferInfo");
    message.set("console_output", handle_variant(console_output));
    message.set("result", Variant::boolean(result));
    if (result) {
      message.set("size", arena.new_coord(console_screen_buffer_info->dwSize));
      message.set("cursor_position", arena.new_coord(console_screen_buffer_info->dwCursorPosition));
      message.set("attributes", console_screen_buffer_info->wAttributes);
      message.set("window", arena.new_small_rect(console_screen_buffer_info->srWindow));
      message.set("maximum_window_size", arena.new_coord(console_screen_buffer_info->dwMaximumWindowSize));
    }
  END_LOG_MESSAGE();
  return result;
}

dword_t LoggingConsole::get_console_title_a(ansi_str_t console_title,
    dword_t size) {
  dword_t result = delegate().get_console_title_a(console_title, size);
  BEGIN_LOG_MESSAGE("GetConsoleTitleA");
    message.set("size", size);
    message.set("result", result);
    if (result)
      message.set("console_title", console_title);
  END_LOG_MESSAGE();
  return result;
}

dword_t LoggingConsole::get_console_title_w(wide_str_t console_title,
    dword_t size) {
  dword_t result = delegate().get_console_title_w(console_title, size);
  BEGIN_LOG_MESSAGE("GetConsoleTitleW");
    message.set("size", size);
    message.set("result", result);
    if (result)
      message.set("console_title", arena.new_utf16(console_title, result));
  END_LOG_MESSAGE();
  return result;
}

hwnd_t LoggingConsole::get_console_window() {
  hwnd_t result = delegate().get_console_window();
  BEGIN_LOG_MESSAGE("GetConsoleWindow");
    message.set("result", handle_variant(result));
  END_LOG_MESSAGE();
  return result;
}

handle_t LoggingConsole::get_std_handle(dword_t std_handle) {
  handle_t result = delegate().get_std_handle(std_handle);
  BEGIN_LOG_MESSAGE("GetStdHandle");
    message.set("std_handle", std_handle);
    message.set("result", handle_variant(result));
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::read_console_a(handle_t console_input, void *buffer,
    dword_t number_of_chars_to_read, dword_t *number_of_chars_read,
    console_readconsole_control_t *input_control) {
  bool_t result = delegate().read_console_a(console_input, buffer,
      number_of_chars_to_read, number_of_chars_read, input_control);
  BEGIN_LOG_MESSAGE("ReadConsoleA");
    message.set("number_of_chars_to_read", number_of_chars_to_read);
    message.set("result", Variant::boolean(result));
    if (result) {
      message.set("buffer", arena.new_utf16(buffer, *number_of_chars_read));
      message.set("number_of_chars_read", *number_of_chars_read);
    }
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::read_console_w(handle_t console_input, void *buffer,
    dword_t number_of_chars_to_read, dword_t *number_of_chars_read,
    console_readconsole_control_t *input_control) {
  bool_t result = delegate().read_console_w(console_input, buffer,
      number_of_chars_to_read, number_of_chars_read, input_control);
  BEGIN_LOG_MESSAGE("ReadConsoleW");
    message.set("number_of_chars_to_read", number_of_chars_to_read);
    message.set("result", Variant::boolean(result));
    if (result) {
      message.set("buffer", arena.new_utf16(buffer, *number_of_chars_read));
      message.set("number_of_chars_read", *number_of_chars_read);
    }
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::read_console_output_a(handle_t console_output,
    char_info_t *buffer, coord_t buffer_size, coord_t buffer_coord,
    small_rect_t *read_region) {
  small_rect_t read_region_in = *read_region;
  bool_t result = delegate().read_console_output_a(console_output, buffer,
      buffer_size, buffer_coord, read_region);
  BEGIN_LOG_MESSAGE("ReadConsoleOutputA");
    message.set("console_output", handle_variant(console_output));
    message.set("buffer_size", arena.new_coord(buffer_size));
    message.set("buffer_coord", arena.new_coord(buffer_coord));
    message.set("read_region_in", arena.new_small_rect(read_region_in));
    message.set("result", Variant::boolean(result));
    if (result) {
      Array chars = arena.new_array();
      for (int y = 0; y < buffer_size.Y; y++) {
        for (int x = 0; x < buffer_size.X; x++) {
          char_info_t info = buffer[x + y * buffer_size.X];
          chars.add(Variant::string(&info.Char.AsciiChar, 1));
          chars.add(info.Attributes);
        }
      }
      message.set("buffer", chars);
      message.set("read_region_out", arena.new_small_rect(*read_region));
    }
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::read_console_output_w(handle_t console_output,
    char_info_t *buffer, coord_t buffer_size, coord_t buffer_coord,
    small_rect_t *read_region) {
  small_rect_t read_region_in = *read_region;
  bool_t result = delegate().read_console_output_w(console_output, buffer,
      buffer_size, buffer_coord, read_region);
  BEGIN_LOG_MESSAGE("ReadConsoleOutputW");
    message.set("console_output", handle_variant(console_output));
    message.set("buffer_size", arena.new_coord(buffer_size));
    message.set("buffer_coord", arena.new_coord(buffer_coord));
    message.set("read_region_in", arena.new_small_rect(read_region_in));
    message.set("result", Variant::boolean(result));
    if (result) {
      Array chars = arena.new_array();
      for (int y = 0; y < buffer_size.Y; y++) {
        for (int x = 0; x < buffer_size.X; x++) {
          char_info_t info = buffer[x + y * buffer_size.X];
          chars.add(arena.new_utf16(&info.Char.UnicodeChar, 1));
          chars.add(info.Attributes);
        }
      }
      message.set("buffer", chars);
      message.set("read_region_out", arena.new_small_rect(*read_region));
    }
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::set_console_cursor_info(handle_t console_output,
    const console_cursor_info_t *console_cursor_info) {
  bool_t result = delegate().set_console_cursor_info(console_output,
      console_cursor_info);
  BEGIN_LOG_MESSAGE("SetConsoleCursorInfo");
    message.set("console_output", handle_variant(console_output));
    message.set("size", console_cursor_info->dwSize);
    message.set("visible", Variant::boolean(console_cursor_info->bVisible));
    message.set("result", Variant::boolean(result));
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::set_console_cursor_position(handle_t console_output,
    coord_t cursor_position) {
  bool_t result = delegate().set_console_cursor_position(console_output,
      cursor_position);
  BEGIN_LOG_MESSAGE("SetConsoleCursorPosition");
    message.set("console_output", handle_variant(console_output));
    message.set("cursor_position", arena.new_coord(cursor_position));
    message.set("result", Variant::boolean(result));
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::set_console_mode(handle_t console_handle, dword_t mode) {
  bool_t result = delegate().set_console_mode(console_handle, mode);
  BEGIN_LOG_MESSAGE("GetConsoleMode");
    message.set("console_handle", handle_variant(console_handle));
    message.set("mode", mode);
    message.set("result", Variant::boolean(result));
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::set_console_title_a(ansi_cstr_t console_title) {
  bool_t result = delegate().set_console_title_a(console_title);
  BEGIN_LOG_MESSAGE("SetConsoleTitleA");
    message.set("console_title", console_title);
    message.set("result", Variant::boolean(result));
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::set_console_title_w(wide_cstr_t console_title) {
  bool_t result = delegate().set_console_title_w(console_title);
  BEGIN_LOG_MESSAGE("SetConsoleTitleW");
    message.set("console_title", arena.new_utf16(console_title,
        String::wstrlen(console_title)));
    message.set("result", Variant::boolean(result));
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::write_console_a(handle_t console_output, const void *buffer,                            \
       dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
       void *reserved) {
  bool_t result = delegate().write_console_a(console_output, buffer,
      number_of_chars_to_write, number_of_chars_written, reserved);
  BEGIN_LOG_MESSAGE("WriteConsoleA");
    message.set("number_of_chars_to_write", number_of_chars_to_write);
    message.set("buffer", Variant::string(static_cast<const char*>(buffer),
        (uint32_t) number_of_chars_to_write));
    message.set("result", Variant::boolean(result));
    if (result)
      message.set("number_of_chars_written", *number_of_chars_written);
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::write_console_w(handle_t console_output, const void *buffer,                            \
       dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
       void *reserved) {
  bool_t result = delegate().write_console_w(console_output, buffer,
      number_of_chars_to_write, number_of_chars_written, reserved);
  BEGIN_LOG_MESSAGE("WriteConsoleW");
    message.set("number_of_chars_to_write", number_of_chars_to_write);
    message.set("buffer", arena.new_utf16(buffer, number_of_chars_to_write));
    message.set("result", Variant::boolean(result));
    if (result)
      message.set("number_of_chars_written", *number_of_chars_written);
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::write_console_output_a(handle_t console_output,
    const char_info_t *buffer, coord_t buffer_size, coord_t buffer_coord,
    small_rect_t *write_region) {
  small_rect_t write_region_in = *write_region;
  bool_t result = delegate().write_console_output_a(console_output, buffer,
      buffer_size, buffer_coord, write_region);
  BEGIN_LOG_MESSAGE("WriteConsoleOutputA");
    message.set("console_output", handle_variant(console_output));
    message.set("buffer_size", arena.new_coord(buffer_size));
    Array chars = arena.new_array();
    for (int y = 0; y < buffer_size.Y; y++) {
      for (int x = 0; x < buffer_size.X; x++) {
        char_info_t info = buffer[x + y * buffer_size.X];
        chars.add(Variant::string(&info.Char.AsciiChar, 1));
        chars.add(info.Attributes);
      }
    }
    message.set("buffer", chars);
    message.set("buffer_coord", arena.new_coord(buffer_coord));
    message.set("write_region_in", arena.new_small_rect(write_region_in));
    message.set("result", Variant::boolean(result));
    if (result)
      message.set("write_region_out", arena.new_small_rect(*write_region));
  END_LOG_MESSAGE();
  return result;
}

bool_t LoggingConsole::write_console_output_w(handle_t console_output,
    const char_info_t *buffer, coord_t buffer_size, coord_t buffer_coord,
    small_rect_t *write_region) {
  small_rect_t write_region_in = *write_region;
  bool_t result = delegate().write_console_output_w(console_output, buffer,
      buffer_size, buffer_coord, write_region);
  BEGIN_LOG_MESSAGE("WriteConsoleOutputW");
    message.set("console_output", handle_variant(console_output));
    message.set("buffer_size", arena.new_coord(buffer_size));
    Array chars = arena.new_array();
    for (int y = 0; y < buffer_size.Y; y++) {
      for (int x = 0; x < buffer_size.X; x++) {
        char_info_t info = buffer[x + y * buffer_size.X];
        chars.add(arena.new_utf16(&info.Char.UnicodeChar, 1));
        chars.add(info.Attributes);
      }
    }
    message.set("buffer", chars);
    message.set("buffer_coord", arena.new_coord(buffer_coord));
    message.set("write_region_in", arena.new_small_rect(write_region_in));
    message.set("result", Variant::boolean(result));
    if (result)
      message.set("write_region_out", arena.new_small_rect(*write_region));
  END_LOG_MESSAGE();
  return result;
}

#ifdef IS_MSVC
#  include "confront-msvc.cc"
#else
#  include "confront-fallback.cc"
#endif
