//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Type declarations used by the console api.

#ifndef _AGENT_LPC_HH
#define _AGENT_LPC_HH

#include "agent/binpatch.hh"
#include "agent/conapi-types.hh"
#include "c/stdc.h"
#include "utils/callback.hh"

#define FOR_EACH_LPC_FUNCTION(F)                                               \
  F(NtRequestWaitReplyPort, nt_request_wait_reply_port, ntstatus_t,            \
     (handle_t port_handle, lpc::message_data_t *request, lpc::message_data_t *incoming_reply), \
     (port_handle, request, incoming_reply))

namespace lpc {

struct message_data_t;
struct capture_buffer_data_t;
class Message;

class AddressXform {
public:
  AddressXform() : delta_(0) { }
  void initialize(ssize_t tdelta) { delta_ = tdelta; }
  template <typename T> T *remote_to_local(T *arg) { return (arg == NULL) ? NULL : reinterpret_cast<T*>(reinterpret_cast<address_arith_t>(arg) + delta_); }
private:
  ssize_t delta_;
};

// The amount of stack to look at to try to determine the location of
// ConsoleClientCallServer.
#define kLocateCCCSStackSize 5

// An interceptor deals with the full process of replacing the implementation of
// NtRequestWaitReplyPort. There's a few steps to getting everything set up
// properly and an interceptor deals with all that.
class Interceptor {
public:
  typedef tclib::callback_t<fat_bool_t(Interceptor*, lpc::Message*, lpc::message_data_t*)> handler_t;

  // Creates an interceptor that will delegate intercepted calls to the given
  // handler.
  Interceptor(handler_t handler);

  // Installs the interceptor.
  fat_bool_t install();

  // Restores everything to the way it was before the interceptor was installed.
  fat_bool_t uninstall();

  // Disables redirection for the lifetime of instances, restoring the previous
  // value on destruction.
  class Disable {
  public:
    Disable(Interceptor* interceptor);
    ~Disable();
  private:
    Interceptor *interceptor_;
    bool was_enabled_;
  };

private:
  typedef ntstatus_t (*console_client_call_server_f)(void *message,
      void *capture_buffer, ulong_t api_number, ulong_t request_length);

  // Sends the calibration message through the built-in message system to set
  // the internal state of the console properly. This must be a static function;
  // see why in process_locate_cccs_message.
  static fat_bool_t calibrate(Interceptor *interceptor);

  // Does the remaining work of calibrate after it no longer needs to be static.
  fat_bool_t infer_calibration_from_cccs();

  // Processes a stack trace to extract the address of ConsoleClientCallServer,
  // if possible.
  fat_bool_t process_locate_cccs_message(handle_t port_handle, void **stack);

  // Process the calibration message sent after we've located CCCS.
  fat_bool_t process_calibration_message(handle_t port_handle, message_data_t *message);

  // Initialize the given patch.
  fat_bool_t initialize_patch(conprx::PatchRequest *request, const char *name,
      address_t replacement, module_t ntdll);

  // Store the patch requests together in an array.
#define __ADD_ONE__(Name, name, RET, SIG, ARGS) + 1
  static const size_t kPatchRequestCount = 0 FOR_EACH_LPC_FUNCTION(__ADD_ONE__);
  conprx::PatchRequest patch_requests_[0 FOR_EACH_LPC_FUNCTION(__ADD_ONE__)];
#undef __ADD_ONE__

  // This enum is mainly there to produce an index for each patch request that
  // can be used to get them from the array.
  enum patch_request_key_t {
    prFirst = -1
#define __DECLARE_REQUEST_KEY__(Name, name, RET, SIG, ARGS) , pr##Name
    FOR_EACH_LPC_FUNCTION(__DECLARE_REQUEST_KEY__)
#undef __DECLARE_REQUEST_KEY__
  };

  // Declare the state and functions used to interact with each function to
  // patch.
#define __DECLARE_REPLACEMENT_FUNCTION__(Name, name, RET, SIG, ARGS)           \
  typedef RET (*name##_f) SIG;                                                 \
  static RET NTAPI name##_bridge SIG;                                          \
  RET NTAPI name SIG;                                                          \
  conprx::PatchRequest *name##_patch() { return &patch_requests_[pr##Name]; }  \
  RET name##_imposter SIG { return (name##_patch()->get_imposter<name##_f>()) ARGS; }
  FOR_EACH_LPC_FUNCTION(__DECLARE_REPLACEMENT_FUNCTION__)
#undef __DECLARE_REPLACEMENT_FUNCTION__

  // A message number that, if seen by the message infrastructure, is used to
  // configure the infrastructure and not sent.
  static const ulong_t kGetConsoleCPApiNumber = 0x3C;

  handler_t handler_;
  bool enabled_;

  // Data associated with locating ConsoleClientCallServer.
  bool is_locating_cccs_;
  fat_bool_t locate_cccs_result_;
  console_client_call_server_f console_client_call_server_;
  handle_t locate_cccs_port_handle_;

  // Data associated with the calibration message.
  static const ulong_t kCalibrationApiNumber = 0xDECADE;
  bool is_calibrating_;
  fat_bool_t calibrate_result_;
  capture_buffer_data_t *calibration_capture_buffer_;

  // The transform to apply when sending addresses through the console port.
  AddressXform port_xform_;
  AddressXform *port_xform() { return &port_xform_; }
  handle_t console_server_port_handle_;

  conprx::PatchSet *patches() { return &patches_; }
  conprx::PatchSet patches_;

  static Interceptor *current_;
  static Interceptor *current() { return current_; }
};

// The lpc message types should match internal types used by the LPC system.
// They've been pieced together from a bunch of different places, including
// ReactOS and random blog posts.

typedef uint8_t client_id_t[IF_32_BIT(8, 16)];

// The generic part of an LPC message.
struct port_message_data_t {
  union {
    struct {
      ushort_t data_length;
      ushort_t total_length;
    } s1;
    ulong_t length;
  } u1;
  union {
    struct {
      ushort_t type;
      ushort_t data_info_offset;
    } s2;
    ulong_t zero_init;
  };
  union {
    client_id_t client_id;
    uint64_t force_alignment;
  };
  ulong_t message_id;
  union {
    size_t client_view_size;
    ulong_t callback_id;
  };
};

// The header of a capture buffer.
struct capture_buffer_data_t {
  ulong_t length;
  capture_buffer_data_t *related_capture_buffer;
  ulong_t count_message_pointers;
  char *free_space;
  ulong_t *message_pointer_offsets[1];
};

struct get_cp_m {
  uint64_t code_page_id;
  bool_t output;
};

union message_payload_t {
  get_cp_m get_cp;
};

// A console api message, a superset of a port message.
struct message_data_t {
  port_message_data_t header;
  capture_buffer_data_t *capture_buffer;
  ulong_t api_number;
  int32_t return_value;
  ulong_t reserved;
  message_payload_t payload;
};

// A wrapper around a capture buffer. Unlike the underlying data you can always
// get a capture buffer wrapper for a message because the wrapper checks for the
// there-is-none case explicitly and returns sensible values.
class CaptureBuffer {
public:
  CaptureBuffer(capture_buffer_data_t *remote_data, AddressXform *xform, message_data_t *message)
    : remote_data_(remote_data)
    , xform_(xform)
    , message_(message) { }

  // Returns the number of blocks that have been allocated within this buffer.
  ulong_t count() { return remote_data_ == NULL ? 0 : local_data()->count_message_pointers; }

  // Returns the index'th block of data within this buffer.
  tclib::Blob block(size_t index);

private:
  capture_buffer_data_t *remote_data_;
  capture_buffer_data_t *remote_data() { return remote_data_; }
  capture_buffer_data_t *local_data() { return xform()->remote_to_local(remote_data()); }
  AddressXform *xform_;
  AddressXform *xform() { return xform_; }
  message_data_t *message_;
  message_data_t *message() { return message_; }
};

// A wrapper around an LPC message that encapsulates access to the various
// fields of interest.
class Message {
public:
  Message(message_data_t *data, AddressXform *xform);

  // The total size of the message data, including header and full payload.
  size_t total_size() { return data()->header.u1.s1.total_length; }

  // Just the size of the payload, excluding the header size.
  ulong_t data_size() { return data()->header.u1.s1.data_length; }

  ulong_t api_number() { return data()->api_number; }
  ushort_t dll_index() { return static_cast<ushort_t>((api_number() >> 16) & 0xFFFF); }
  ushort_t api_index() { return static_cast<ushort_t>(api_number() & 0xFFFF); }

  message_data_t *data() { return data_; }

  message_payload_t *payload() { return &data_->payload; }

  // Returns a pointer to the capture buffer stored in the message.
  CaptureBuffer *capture_buffer() { return &capbuf_; }

private:
  message_data_t *data_;
  CaptureBuffer capbuf_;
};

} // namespace lpc

#endif // _AGENT_LPC_HH
