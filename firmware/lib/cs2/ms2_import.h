// Lectura de las locomotoras guardadas en la Mobile Station 2.
//
// Cuando se pone en la vía una loco mfx nueva, la MS2 la da de alta (le
// asigna una dirección y lee del decodificador su nombre y sus funciones).
// Después cualquier equipo del bus puede pedirle sus datos, igual que hace
// una Central Station con las MS2 conectadas a ella:
//   1. "lokliste" (MS2 3.x o posterior) o "loknamen" + "inicio cantidad"
//      (versiones antiguas): lista de nombres.
//   2. "lokinfo" + nombre (16 bytes): ficha de cada loco con dirección,
//      protocolo y el tipo de cada función (luz, sonido, bocina...).
// La MS2 responde con una secuencia de configuración (orden 0x21) de texto
// sin comprimir. Basado en clone-ms2-loco.c de can2udp (Gerhard Bertelsmann).
//
// Portable: el firmware le pasa las tramas del bus y una función de envío.
#pragma once

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "marklin_can.h"

namespace cs2 {

// Tipo de función como en la CS2/MS2: símbolo (1 = luces, 5 = sonido,
// 10 = bocina...) y bit 0x80 = función momentánea (solo mientras se pulsa).
constexpr uint8_t kFnMomentary = 0x80;

struct ImportedLoco {
  std::string name;
  uint32_t uid = 0;
  uint32_t mfxUid = 0;
  std::vector<uint8_t> fn;  // fn[n] = tipo de la función Fn (0 = no tiene)
};

// Quita los espacios de relleno que añade la MS2 tras cada salto de línea
// para alinear las líneas a 8 bytes.
std::string stripMs2Spaces(const std::string& in);
// Nombres de una respuesta a "lokliste"/"loknamen" (" .name=") y número
// total de locos (" .wert=", -1 si no viene).
std::vector<std::string> parseLocoNames(const std::string& text, int* total);
// Locos de un fichero "lokinfo" o lokomotive.cs2 (formato MS2 o CS2).
std::vector<ImportedLoco> parseLocos(const std::string& text);

// Recibe una secuencia de configuración (tramas 0x21).
class StreamReceiver {
 public:
  void reset() { active_ = false; }
  // true cuando el fichero está completo (en text()).
  bool feed(const marklin::CanFrame& f);
  const std::string& text() const { return text_; }
  bool crcOk() const { return crcOk_; }
  bool compressed() const { return compressed_; }

 private:
  bool active_ = false;
  uint32_t length_ = 0;
  uint16_t crc_ = 0;
  std::string buf_;
  std::string text_;
  bool crcOk_ = false;
  bool compressed_ = false;
};

class Ms2Importer {
 public:
  enum class State : uint8_t { Idle, List, Info, Done, Failed };

  // Envía una trama al bus.
  std::function<bool(const marklin::CanFrame&)> send;

  // `v3`: la MS2 tiene la versión 3.x o posterior (petición "lokliste").
  void start(uint16_t hash, bool v3, uint32_t nowMs);
  void handleFrame(const marklin::CanFrame& f, uint32_t nowMs);
  void tick(uint32_t nowMs);

  State state() const { return state_; }
  bool busy() const { return state_ == State::List || state_ == State::Info; }
  const std::vector<ImportedLoco>& locos() const { return locos_; }
  const std::vector<std::string>& names() const { return names_; }
  size_t current() const { return next_; }
  const std::string& error() const { return error_; }

  static constexpr uint32_t kTimeoutMs = 4000;

 private:
  void requestList();
  void requestInfo();
  void request(const char* name, const std::vector<std::string>& extra);
  void fileReceived(const std::string& text);
  void fail(const char* why);

  State state_ = State::Idle;
  uint16_t hash_ = 0;
  bool v3_ = true;
  uint32_t sentMs_ = 0;
  int retries_ = 0;
  int total_ = -1;
  size_t next_ = 0;
  std::vector<std::string> names_;
  std::vector<ImportedLoco> locos_;
  std::string error_;
  StreamReceiver rx_;
};

}  // namespace cs2
