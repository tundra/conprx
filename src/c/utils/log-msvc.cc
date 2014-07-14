//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "utils/log-messages.h"
#include "utils/types.hh"

// Ensure that we link with the event log functions.
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32.lib")

// A log backend that writes to a particular host:port.
class SocketLogBackend : public LogBackend {
public:
  SocketLogBackend(const char *host, int port);
  ~SocketLogBackend();
  virtual void vrecord(Log::Type type, const char *file, int line,
      const char *fmt, va_list args);
private:
  SOCKET socket_;
};

SocketLogBackend::SocketLogBackend(const char *hostname, int port)
  : socket_(INVALID_SOCKET) {
  WSADATA wsadata;
  if (WSAStartup(MAKEWORD(2,2), &wsadata) != NO_ERROR) {
    fprintf(stderr, "Error calling WSAStartup()\n");
    fflush(stderr);
    return;
  }

  socket_ = socket(PF_INET, SOCK_STREAM, 0);
  if (socket_ == INVALID_SOCKET) {
    fprintf(stderr, "Error calling socket()\n");
    fflush(stderr);
    return;
  }

  struct hostent *hostent = gethostbyname(hostname);
  if (hostent == NULL) {
    fprintf(stderr, "Error calling gethostbyname()\n");
    fflush(stderr);
    return;
  }
  unsigned long ip_addr = *reinterpret_cast<unsigned long*>(hostent->h_addr_list[0]);

  struct sockaddr_in addr;
  ZeroMemory(&addr, sizeof(addr));
  addr.sin_family = PF_INET;
  addr.sin_addr.s_addr = ip_addr;
  addr.sin_port = htons(port);

  if (connect(socket_, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
    fprintf(stderr, "Error calling bind(): %i\n", GetLastError());
    fflush(stderr);
    return;
  }
}

SocketLogBackend::~SocketLogBackend() {
  closesocket(socket_);
}

void SocketLogBackend::vrecord(Log::Type type, const char *file, int line,
    const char *fmt, va_list args) {
  // Format the varargs into a message.
  ansi_char_t message_string[kMaxLogMessageSize];
  int size = vsnprintf(message_string, kMaxLogMessageSize, fmt, args);

  send(socket_, message_string, size, 0);
}

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

static SocketLogBackend *socket_log_instance = NULL;

static void dispose_socket_log_instance() {
  SocketLogBackend *instance = socket_log_instance;
  socket_log_instance = NULL;
  delete instance;
}

LogBackend *LogBackend::get_default() {
  if (socket_log_instance == NULL) {
    socket_log_instance = new SocketLogBackend("aa00", 7000);
    atexit(dispose_socket_log_instance);
  }
  return socket_log_instance;
}
