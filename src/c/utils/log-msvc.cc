//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "utils/log-messages.h"
#include "utils/types.hh"

// Ensure that we link with the event log functions.
#pragma comment(lib, "advapi32.lib")

#define EVENT_SOURCE_NAME TEXT("ConsoleProxyAgent")


// Fallback log that only uses standard C functions that should be available on
// all platforms.
class WindowsEventLogBackend : public LogBackend {
public:
  WindowsEventLogBackend();
  virtual void vrecord(Log::Type type, ansi_cstr_t file, int line,
      ansi_cstr_t fmt, va_list args);
private:
  handle_t event_source_;
};

WindowsEventLogBackend::WindowsEventLogBackend()
  : event_source_(NULL) {
  event_source_ = RegisterEventSource(
      NULL,               // lpUNCServerName
      EVENT_SOURCE_NAME); // lpSourceName
}

void WindowsEventLogBackend::vrecord(Log::Type type, ansi_cstr_t file, int line,
    ansi_cstr_t fmt, va_list args) {
  // Format the varargs into a message.
  ansi_char_t message_string[kMaxLogMessageSize];
  vsnprintf(message_string, kMaxLogMessageSize, fmt, args);

  // Convert the line number to a string.
  ansi_char_t line_string[kMaxLogMessageSize];
  sprintf(line_string, "%i", line);

  // Find the right event id to use.
  dword_t event_id = 0;
  switch (type) {
  case Log::ERR:
    event_id = MSG_LOG_ERROR;
    break;
  case Log::WRN:
    event_id = MSG_LOG_WARNING;
    break;
  case Log::DBG:
    event_id = MSG_LOG_VERBOSE;
    break;
  default:
    event_id = MSG_LOG_INFO;
    break;
  }

#define kStringCount 3
  // Send the message off to the event log.
  ansi_cstr_t strings[kStringCount] = {message_string, file, line_string};
  ReportEventA(
      event_source_,    // hEventLog
      type,             // wType
      GENERIC_CATEGORY, // wCategory
      event_id,         // dwEventID
      NULL,             // lpUserSid
      kStringCount,     // wNumStrings
      0,                // dwDataSize
      strings,          // lpStrings
      NULL);            // lpRawData
}

LogBackend *LogBackend::get_default() {
  static WindowsEventLogBackend *instance = NULL;
  if (instance == NULL)
    instance = new WindowsEventLogBackend();
  return instance;
}
