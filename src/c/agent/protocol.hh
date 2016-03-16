//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Type declarations used by both sides of the console api.

#ifndef _AGENT_PROTOCOL_HH
#define _AGENT_PROTOCOL_HH

#define FOR_EACH_LPC_TO_INTERCEPT(F)                                           \
  F(GetConsoleCP,       get_console_cp,         0,      0x3C)                  \
  F(SetConsoleCP,       set_console_cp,         0,      0x3D)                  \
  F(GetConsoleTitle,    get_console_title,      0,      0x24)                  \
  F(SetConsoleTitle,    set_console_title,      0,      0x25)

#define FOR_EACH_OTHER_KNOWN_LPC(F)                                            \
  F(BaseDllInitHelper,  ,                       0,      76)                    \
  F(GetFileType,        ,                       0,      35)

namespace conprx {

// The result of a windows-like call: either a successful value or an nonzero
// error code indicating a problem.
template <typename T>
class generic_response_t {
public:
  // Creates a successful response whose value is the default for T.
  generic_response_t() : error_(0), value_(T()) { }

  // Does this response indicate a problem?
  bool has_error() const { return error_ != 0; }

  // The error code, 0 if this response is successful.
  dword_t error_code() const { return error_; }

  // The value, undefined if this response was not successful.
  T value() const { return value_; }

  // Store the error code in the given out parameter and return either the value
  // if this was successful or the given default if it was not. Note that the
  // error code is stored in both cases, 0 if successful.
  T flush(const T &defawlt, dword_t *error_out);

protected:
  generic_response_t(dword_t error, T value) : error_(error), value_(value) { }

private:
  dword_t error_;
  T value_;
};

template <typename T>
T generic_response_t<T>::flush(const T &defawlt, dword_t *error_out) {
  *error_out = error_;
  return has_error() ? defawlt : value_;
}

template <typename T>
class response_t : public generic_response_t<T> {
public:
  response_t() : generic_response_t<T>() { }

  // Returns an error response value with the given error code.
  static response_t<T> error(dword_t error) { return response_t<T>(error, T()); }

  template <typename S>
  static response_t<T> error(const response_t<S> &that) { return error(that.error_code()); }

  // Returns a successful response with the given value and a 0 error code.
  static response_t<T> of(const T &value) { return response_t<T>(0, value); }

protected:
  response_t(dword_t error, T value) : generic_response_t<T>(error, value) { }

};

// A bool-response is like a bool response except that it enforces some
// additional constraints because in most contexts a bool response with a false
// value is functionally an error and so the corner case of a successful
// bool response with a false value is usually meaningless. Using a bool-
// response rules out that corner case.
template <>
class response_t<bool_t> : public generic_response_t<bool_t> {
public:
  // Returns an error response value with the given error code.
  static response_t<bool_t> error(dword_t error) { return response_t<bool_t>(error, false); }

  template <typename T>
  static response_t<bool_t> error(const response_t<T> &that) { return error(that.error_code()); }

  // Returns a successful response with the given value and a 0 error code.
  static response_t<bool_t> of(bool_t value) { return response_t<bool_t>(0, true); }

  static response_t<bool_t> yes() { return response_t<bool_t>(0, true); }

private:
  response_t(dword_t error, bool_t value) : generic_response_t<bool_t>(error, value) { }
};

} // namespace conprx

#endif // _AGENT_PROTOCOL_HH
