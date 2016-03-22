//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Type declarations used by both sides of the console api.

#include "rpc.hh"
#include "utils/types.hh"

#ifndef _CONPRX_SHARE_PROTOCOL_HH
#define _CONPRX_SHARE_PROTOCOL_HH

#define FOR_EACH_LPC_TO_INTERCEPT(F)                                           \
  F(GetConsoleMode,     get_console_mode,       0,      0x08)                  \
  F(SetConsoleMode,     set_console_mode,       0,      0x11)                  \
  F(GetConsoleTitle,    get_console_title,      0,      0x24)                  \
  F(SetConsoleTitle,    set_console_title,      0,      0x25)                  \
  F(GetConsoleCP,       get_console_cp,         0,      0x3C)                  \
  F(SetConsoleCP,       set_console_cp,         0,      0x3D)

#define FOR_EACH_OTHER_KNOWN_LPC(F)                                            \
  F(BaseDllInitHelper,  ,                       0,      76)                    \
  F(GetFileType,        ,                       0,      35)

namespace conprx {

// A small subset of code pages.
enum code_page_t {
  cpMsDos = 437,
  cpUtf8 = 65001,
  cpUsAscii = 20127
};

static const int32_t kStdInputHandle = -10;
static const int32_t kStdOutputHandle = -11;
static const int32_t kStdErrorHandle = -12;

// The block of data passed through to the agent's dll connector.
struct connect_data_t {
  int32_t magic;
  standalone_dword_t parent_process_id;
  tclib::naked_file_handle_t agent_in_handle;
  tclib::naked_file_handle_t agent_out_handle;

  // The magic value we expect to find in the magic field if it has been
  // transferred correctly.
  static const int32_t kMagic = 0xFABACAEA;
};

template <typename T> class response_t;

// A wrapper around an nt status code that makes it easier to dissect the value
// and clearer how to treat it when.
class NtStatus {
public:
  enum Severity {
    nsSuccess = 0x00000000,
    nsInfo = 0x40000000,
    nsWarning = 0x80000000,
    nsError = 0xC0000000
  };

  enum Origin {
    noMs = 0x00000000,
    noCustomer = 0x20000000
  };

  // Returns this status appropriately nt-encoded.
  uint32_t to_nt() { return encoded_; }

  // Returns the naked error code value.
  uint32_t code() { return encoded_ & kCodeMask; }

  // Given a generic response, returns the appropriate ntstatus to communicate
  // the state of the response to the nt framwork.
  template <typename T>
  static NtStatus from_response(response_t<T> resp);

  // Given an nt status code, returns the last_error value it represents.
  static dword_t decode_error(int32_t ntstatus);

  static NtStatus from_nt(int32_t value) { return NtStatus(value); }

  static NtStatus from(Severity severity, Origin origin, uint32_t code);

  static NtStatus success() { return NtStatus(0); }

  bool is_success() { return (encoded_ & kFailureMask) == 0; }

private:
  NtStatus(uint32_t encoded) : encoded_(encoded) { }

  uint32_t encoded_;

  static const uint32_t kFailureMask = 0x80000000;
  static const uint32_t kSeverityMask = 0xC0000000;
  static const uint32_t kCodeMask = 0xFFFF;
  static const uint32_t kSeveritySuccess = 0x00000000;
};

template <typename T>
NtStatus NtStatus::from_response(response_t<T> resp) {
  return resp.has_error() ? from(nsError, noCustomer, resp.error_code()) : success();
}

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

// A general response with no extra behavior.
template <typename T>
class response_t : public generic_response_t<T> {
public:
  response_t() : generic_response_t<T>() { }

  // Returns an error response value with the given error code.
  static response_t<T> error(dword_t error) { return response_t<T>(error, T()); }

  // Converts an error response of one type to another.
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

  // Converts an error response of one type to another.
  template <typename T>
  static response_t<bool_t> error(const response_t<T> &that) { return error(that.error_code()); }

  // Returns a successful response with the given value and a 0 error code.
  static response_t<bool_t> of(bool_t value) { return response_t<bool_t>(0, true); }

  static response_t<bool_t> yes() { return response_t<bool_t>(0, true); }

private:
  response_t(dword_t error, bool_t value) : generic_response_t<bool_t>(error, value) { }
};

// A wrapper around a native handle. A handle can either be valid and have a
// non-negative id or invalid.
class Handle {
public:
  // Initializes an invalid handle.
  Handle() : id_(kInvalidHandleValue) { }

  // Initializes a handle with the given id.
  explicit Handle(int64_t id) : id_(id) { }

  // Initializes a handle with an id corresponding to the given pointer.
  explicit Handle(void *raw) : id_(reinterpret_cast<int64_t>(raw)) { }

  // Is this a valid handle?
  bool is_valid() { return id_ != kInvalidHandleValue; }

  // The seed type for handles.
  static plankton::SeedType<Handle> *seed_type() { return &kSeedType; }

  // Returns an invalid handle.
  static Handle invalid() { return Handle(); }

  int64_t id() { return id_; }

  void *ptr() { return reinterpret_cast<void*>(id_); }

private:
  template <typename T> friend class plankton::DefaultSeedType;

  plankton::Variant to_seed(plankton::Factory *factory);
  static Handle *new_instance(plankton::Variant header, plankton::Factory *factory);
  void init(plankton::Seed payload, plankton::Factory *factory);

  // This must match the invalid handle value from windows.
  static const int64_t kInvalidHandleValue = -1;

  static plankton::DefaultSeedType<Handle> kSeedType;
  int64_t id_;
};

} // namespace conprx

#endif // _CONPRX_SHARE_PROTOCOL_HH
