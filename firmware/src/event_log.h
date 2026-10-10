// Registro circular de eventos (monitor CAN, mensajes de scripts, avisos)
// que la interfaz web consulta de forma incremental.
#pragma once

#include <Arduino.h>

enum class LogKind : uint8_t { Info, CanRx, CanTx, Script, Error };

struct LogEntry {
  uint32_t seq = 0;
  uint32_t ms = 0;
  LogKind kind = LogKind::Info;
  char text[100] = {0};
};

class EventLog {
 public:
  static constexpr int kSize = 150;

  void add(LogKind kind, const char* text);
  void addf(LogKind kind, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

  uint32_t lastSeq() const { return seq_; }
  // Recorre las entradas con seq > since (de la más antigua a la más nueva).
  template <typename F>
  void forEachSince(uint32_t since, F fn) const {
    const uint32_t first = seq_ > kSize ? seq_ - kSize + 1 : 1;
    for (uint32_t s = since + 1 < first ? first : since + 1; s <= seq_; s++)
      fn(entries_[s % kSize]);
  }

  // Si está activo, cada entrada se copia también a la consola serie.
  bool echoSerial = true;
  bool echoCan = false;  // incluir tramas CAN en la consola serie

  static const char* kindName(LogKind k);

 private:
  LogEntry entries_[kSize];
  uint32_t seq_ = 0;
};

extern EventLog eventLog;
