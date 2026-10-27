// Interfaz de red de Central Station (CS2/CS3) para apps como RailControl Pro.
//
// La app se conecta por TCP al puerto 15731 del ESP32 (o habla por UDP con
// los puertos 15731/15730). Todo lo que pasa por el bus CAN le llega a la
// app y lo que manda la app va al bus, como en una Central Station; además
// el ESP32 le entrega la lista de locos registradas y los desvíos de la
// maqueta activa (ver lib/cs2/cs2_lan.h y docs/railcontrol-pro.md).
#pragma once

#include <stdint.h>

#include <string>

#include "marklin_can.h"

namespace cs2server {

void setup();
void loop();

// Llamar con cada trama del bus (recibida o enviada por el ESP32).
void onBusFrame(const marklin::CanFrame& f, bool tx);
// Avisa a las apps de un cambio en un sensor local (GPIO) del ESP32.
void notifySensor(uint32_t id, bool active);

// Fichero .cs2 para una petición ("loks", "mags", "gbs", "gbs-0"...); también
// se sirven por HTTP en /config/ como en la CS2.
bool configFile(const std::string& request, std::string* content);

int tcpClients();
int udpPeers();
uint32_t framesFromApps();
uint32_t configRequests();

}  // namespace cs2server
