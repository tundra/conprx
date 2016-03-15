//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Type declarations used by both sides of the console api.

#ifndef _AGENT_PROTOCOL_HH
#define _AGENT_PROTOCOL_HH

#define FOR_EACH_LPC_TO_INTERCEPT(F)                                           \
  F(GetConsoleCP,       get_console_cp,         0,      0x3C)                  \
  F(SetConsoleCP,       set_console_cp,         0,      0x3D)                  \
  F(SetConsoleTitle,    set_console_title,      0,      0x25)

#define FOR_EACH_OTHER_KNOWN_LPC(F)                                            \
  F(BaseDllInitHelper,  ,                       0,      76)                    \
  F(GetFileType,        ,                       0,      35)

namespace conprx {

// The result of a windows-like call: either a successful value or an nonzero
// error code indicating a problem.
template <typename T>
class Response {
public:
  // Creates a successful response whose value is the default for T.
  Response() : error_(0), value_(T()) { }

  // Does this response indicate a problem?
  bool has_error() { return error_ != 0; }

  // The error code, 0 if this response is successful.
  dword_t error() { return error_; }

  // The value, undefined if this response was not successful.
  T value() { return value_; }

  // Store the error code in the given out parameter and return either the value
  // if this was successful or the given default if it was not. Note that the
  // error code is stored in both cases, 0 if successful.
  T flush(const T &defawlt, dword_t *error_out);

  // Returns an error response value with the given error code.
  static Response<T> error(dword_t error) { return Response<T>(error, T()); }

  // Returns a successful response with the given value and a 0 error code.
  static Response<T> of(const T &value) { return Response<T>(0, value); }

private:
  Response(dword_t error, T value) : error_(error), value_(value) { }

  dword_t error_;
  T value_;
};

template <typename T>
T Response<T>::flush(const T &defawlt, dword_t *error_out) {
  *error_out = error_;
  return has_error() ? defawlt : value_;
}

} // namespace conprx

#endif // _AGENT_PROTOCOL_HH
