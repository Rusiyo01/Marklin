// Emulación del interfaz de red de una Central Station (CS2/CS3).
//
// Las apps que manejan una Central Station por la red (RailControl Pro,
// Rocrail, iTrain...) usan el mismo protocolo CAN de la Gleisbox, metido en
// paquetes de 13 bytes (ID de 4 bytes big-endian, DLC, 8 datos):
//   - TCP puerto 15731: conexión permanente, en los dos sentidos.
//   - UDP: la app envía al puerto 15731 y la central responde al 15730.
// Además piden a la central sus ficheros de configuración ("loks" =
// lokomotive.cs2, "mags" = magnetartikel.cs2, "gbs" = gleisbild.cs2...), que
// llegan comprimidos con zlib en una secuencia de tramas (comando 0x21).
//
// Este módulo no depende de Arduino: genera las respuestas y los ficheros; el
// firmware se encarga de los sockets (src/cs2_server.cpp) y los tests del PC
// comprueban el formato.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "marklin_can.h"

namespace marklin {
class Controller;
}

namespace cs2 {

constexpr uint16_t kTcpPort = 15731;
constexpr uint16_t kUdpListenPort = 15731;
constexpr uint16_t kUdpReplyPort = 15730;
constexpr size_t kFrameSize = 13;

// Tipo de equipo que anuncia una Central Station al responder a un ping.
constexpr uint16_t kDeviceTypeCentralStation = 0xFFFF;

void encodeFrame(const marklin::CanFrame& f, uint8_t out[kFrameSize]);
// false si el DLC no es válido (> 8).
bool decodeFrame(const uint8_t in[kFrameSize], marklin::CanFrame* f);

// CRC-CCITT (polinomio 0x1021) usado en las secuencias de configuración.
uint16_t crcCcitt(const uint8_t* data, size_t len, uint16_t seed = 0xFFFF);
uint32_t adler32(const uint8_t* data, size_t len);
// Flujo zlib válido con bloques "stored" (sin comprimir): cualquier
// descompresor zlib lo acepta y no hace falta una librería de compresión.
std::vector<uint8_t> zlibStore(const std::string& data);

// Tramas que responden a la petición de configuración `requestId` con el
// contenido de un fichero: primera trama DLC 6 (longitud + CRC), después
// tramas de 8 bytes con [tamaño descomprimido BE32][zlib] rellenado a 8.
std::vector<marklin::CanFrame> configStream(uint32_t requestId, const std::string& content);

// ---- Modelo de datos para los ficheros .cs2 ---------------------------------

struct LocoEntry {
  std::string name;
  uint32_t uid;
  std::vector<uint8_t> fn;  // tipo de cada función (vacío = genéricas)
};

struct ArticleEntry {
  uint16_t id;       // dirección del accesorio (1..)
  std::string name;
  std::string typ;   // linksweiche | rechtsweiche | dreiwegweiche | ...
  bool dcc;
};

struct ContactEntry {
  uint32_t id;       // contacto S88 (1..)
  std::string name;
};

struct Model {
  std::vector<LocoEntry> locos;
  std::vector<ArticleEntry> articles;
  std::vector<ContactEntry> contacts;
  std::string pageName = "Layout";
  uint32_t uid = 0x43533201;  // UID de la central (número de serie en geraet.vrs)
  std::string ip;              // IP por la que se llega a la central (prefs.cs2)
};

std::string lokomotiveCs2(const std::vector<LocoEntry>& locos);
std::string magnetartikelCs2(const std::vector<ArticleEntry>& articles);
std::string gleisbildCs2(const std::string& pageName);
// Página de plano de vías: un panel con todos los desvíos (con su nombre) y
// los contactos, para poder accionarlos desde la app.
std::string gleisbildPageCs2(const Model& model);
// Descripción del equipo (/config/geraet.vrs); las apps la piden por HTTP
// para comprobar que hay una CS2 en esa IP. Mismo formato que can2lan.
std::string geraetVrs(uint32_t uid);
// Preferencias de la CS2 (/config/prefs.cs2): IP, versión y UID.
std::string prefsCs2(const Model& model);

// Contenido del fichero que corresponde a una petición ("loks", "mags",
// "gbs", "gbs-0", "fs", "lokstat", "geraet"...). false si no es uno de ellos.
bool configFile(const std::string& request, const Model& model, std::string* content);

// ---- Pasarela --------------------------------------------------------------

class Gateway {
 public:
  explicit Gateway(uint32_t uid = 0x43533201) { setUid(uid); }  // "CS2" + 1

  void setUid(uint32_t uid);
  uint32_t uid() const { return uid_; }

  // Respuesta a un ping como Central Station (tipo 0xFFFF, versión 4.2).
  marklin::CanFrame pingReply() const;

  // Procesa una trama que llega de una app. Las respuestas para esa app se
  // añaden a `replies`. Devuelve true si la trama debe ir también al bus CAN
  // (y al resto de apps). `fillModel` solo se llama si piden un fichero.
  bool handleClientFrame(const marklin::CanFrame& f,
                         const std::function<void(Model*)>& fillModel,
                         std::vector<marklin::CanFrame>* replies);

  uint32_t configRequests() const { return configRequests_; }

 private:
  uint32_t uid_ = 0;
  uint16_t hash_ = 0;
  uint32_t configRequests_ = 0;
};

// Respuesta que daría el módulo de vía de la central a una orden de una app
// (estado y STOP/GO, velocidad, sentido, funciones, accesorios), con el estado
// del controlador. Se usa cuando no hay Gleisbox que conteste en el bus.
bool emulateTrackResponse(const marklin::CanFrame& req, const marklin::Controller& c, uint16_t hash,
                          marklin::CanFrame* out);

// Evento de contacto S88 (como los de un Link S88) para avisar a las apps de
// un sensor local del ESP32. id = módulo * 1000 + contacto.
marklin::CanFrame s88Event(uint16_t hash, uint32_t id, bool active);

}  // namespace cs2
