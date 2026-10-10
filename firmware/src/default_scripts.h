// Scripts de ejemplo que se graban en la memoria del ESP32 la primera vez
// que arranca (si no existe ya un script con ese nombre). Se pueden editar
// o borrar desde la interfaz web.
//
// Cambia las direcciones de las locos, sensores y desvíos por las tuyas.
#pragma once

struct DefaultScript {
  const char* name;
  const char* source;
};

static const DefaultScript kDefaultScripts[] = {
    {"demo_tiempo",
     R"(# Demo sin sensores: ida y vuelta por tiempo.
# Ideal para la primera prueba. Usa un tramo de vía recto
# y ajusta los tiempos para que la loco no llegue al final.
loco tren mm2 24        # <- tu loco (mm2, dcc o mfx + dirección)

power on
func tren 0 on          # luces
repeat 5
  dir tren fwd
  ramp tren 30% 2s
  wait 4s
  ramp tren 0 2s
  wait 3s
  dir tren rev
  ramp tren 30% 2s
  wait 4s
  ramp tren 0 2s
  wait 3s
end
func tren 0 off
log demo terminada
)"},

    {"lanzadera",
     R"(# Lanzadera entre dos estaciones con un sensor en cada extremo.
#
#  [sensor 1] ===================== [sensor 2]
#   estación A                      estación B
#
loco tren mm2 24
sensor estacion_a 1
sensor estacion_b 2

power on
func tren 0 on
label inicio
  log saliendo hacia B
  dir tren fwd
  ramp tren 50% 3s
  wait sensor estacion_b on timeout 2m
  ramp tren 0 1500ms
  func tren 2 on          # bocina (según decodificador)
  wait 500ms
  func tren 2 off
  wait random 10s 30s     # parada en la estación

  log saliendo hacia A
  dir tren rev
  ramp tren 50% 3s
  wait sensor estacion_a on timeout 2m
  ramp tren 0 1500ms
  wait random 10s 30s
goto inicio
)"},

    {"puente_tren1",
     R"(# Dos trenes en circuitos distintos que comparten un puente de vía
# única. Arranca "puente_tren1" y "puente_tren2" a la vez.
# Cada tren reserva el puente con 'lock' antes de entrar y lo libera
# con 'unlock' al salir; si está ocupado, frena y espera.
loco tren1 mm2 10
sensor antes_puente 3
sensor despues_puente 4

power on
dir tren1 fwd
ramp tren1 45% 3s
label vuelta
  wait sensor antes_puente on
  if locked puente goto esperar
  lock puente
  goto cruzar
label esperar
  ramp tren1 0 1s
  log puente ocupado, esperando
  lock puente
  ramp tren1 45% 3s
label cruzar
  wait sensor despues_puente on timeout 1m
  unlock puente
goto vuelta
)"},

    {"puente_tren2",
     R"(# Segundo tren del ejemplo del puente (ver puente_tren1).
loco tren2 mm2 11
sensor antes_puente 5
sensor despues_puente 6

power on
dir tren2 fwd
ramp tren2 45% 3s
label vuelta
  wait sensor antes_puente on
  if locked puente goto esperar
  lock puente
  goto cruzar
label esperar
  ramp tren2 0 1s
  log puente ocupado, esperando
  lock puente
  ramp tren2 45% 3s
label cruzar
  wait sensor despues_puente on timeout 1m
  unlock puente
goto vuelta
)"},

    {"estacion_aleatoria",
     R"(# Circuito con una estación de dos vías. En cada vuelta el tren
# elige al azar la vía 1 o la 2 (desvío 1) y espera un tiempo variable.
loco tren dcc 3
sensor llegada 7         # sensor a la entrada de la estación
sensor anden 8           # sensor en el punto de parada

power on
func tren 0 on
dir tren fwd
label vuelta
  ramp tren 60% 3s
  wait sensor llegada on
  if chance 50 goto via2
  switch 1 straight
  log entrando por la via 1
  goto parar
label via2
  switch 1 round
  log entrando por la via 2
label parar
  ramp tren 25% 1s
  wait sensor anden on timeout 1m
  speed tren 0
  wait random 15s 45s
goto vuelta
)"},
};

static const int kDefaultScriptCount = sizeof(kDefaultScripts) / sizeof(kDefaultScripts[0]);
