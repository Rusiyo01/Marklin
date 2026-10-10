#include "event_log.h"

#include <stdarg.h>

EventLog eventLog;

void EventLog::add(LogKind kind, const char* text) {
  seq_++;
  LogEntry& e = entries_[seq_ % kSize];
  e.seq = seq_;
  e.ms = millis();
  e.kind = kind;
  strncpy(e.text, text, sizeof(e.text) - 1);
  e.text[sizeof(e.text) - 1] = 0;

  const bool isCan = kind == LogKind::CanRx || kind == LogKind::CanTx;
  if (echoSerial && (!isCan || echoCan)) {
    Serial.printf("[%7.1f] %-6s %s\n", e.ms / 1000.0, kindName(kind), e.text);
  }
}

void EventLog::addf(LogKind kind, const char* fmt, ...) {
  char buf[sizeof(LogEntry::text)];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  add(kind, buf);
}

const char* EventLog::kindName(LogKind k) {
  switch (k) {
    case LogKind::Info: return "info";
    case LogKind::CanRx: return "can<";
    case LogKind::CanTx: return "can>";
    case LogKind::Script: return "script";
    case LogKind::Error: return "error";
  }
  return "?";
}
