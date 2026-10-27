// Scripts de ejemplo (en inglés, como la interfaz) que se graban en la
// memoria del ESP32 la primera vez que arranca (si no existe ya un script con
// ese nombre). Se pueden editar o borrar desde la interfaz web.
//
// Cambia las direcciones de las locos, sensores y desvíos por las tuyas.
#pragma once

struct DefaultScript {
  const char* name;
  const char* source;
};

static const DefaultScript kDefaultScripts[] = {
    {"demo_time",
     R"(# Demo without sensors: back and forth by time.
# Ideal for the first test. Use a straight section of track
# and adjust the times so the loco does not reach the end.
loco train mm2 24       # <- your loco (mm2, dcc or mfx + address)

power on
func train 0 on         # lights
repeat 5
  dir train fwd
  ramp train 30% 2s
  wait 4s
  ramp train 0 2s
  wait 3s
  dir train rev
  ramp train 30% 2s
  wait 4s
  ramp train 0 2s
  wait 3s
end
func train 0 off
log demo finished
)"},

    {"shuttle",
     R"(# Shuttle between two stations with a sensor at each end.
#
#  [sensor 1] ===================== [sensor 2]
#   station A                       station B
#
loco train mm2 24
sensor station_a 1
sensor station_b 2

power on
func train 0 on
label start
  log leaving for B
  dir train fwd
  ramp train 50% 3s
  wait sensor station_b on timeout 2m
  ramp train 0 1500ms
  func train 2 on         # horn (depends on the decoder)
  wait 500ms
  func train 2 off
  wait random 10s 30s     # stop at the station
  log leaving for A
  dir train rev
  ramp train 50% 3s
  wait sensor station_a on timeout 2m
  ramp train 0 1500ms
  wait random 10s 30s
goto start
)"},

    {"bridge_train1",
     R"(# Two trains on separate loops sharing a single-track bridge.
# Start "bridge_train1" and "bridge_train2" at the same time.
# Each train reserves the bridge with 'lock' before entering and frees
# it with 'unlock' when leaving; if it is taken, it brakes and waits.
loco train1 mm2 10
sensor before_bridge 3
sensor after_bridge 4

power on
dir train1 fwd
ramp train1 45% 3s
label lap
  wait sensor before_bridge on
  if locked bridge goto wait_bridge
  lock bridge
  goto cross
label wait_bridge
  ramp train1 0 1s
  log bridge taken, waiting
  lock bridge
  ramp train1 45% 3s
label cross
  wait sensor after_bridge on timeout 1m
  unlock bridge
goto lap
)"},

    {"bridge_train2",
     R"(# Second train of the bridge example (see bridge_train1).
loco train2 mm2 11
sensor before_bridge 5
sensor after_bridge 6

power on
dir train2 fwd
ramp train2 45% 3s
label lap
  wait sensor before_bridge on
  if locked bridge goto wait_bridge
  lock bridge
  goto cross
label wait_bridge
  ramp train2 0 1s
  log bridge taken, waiting
  lock bridge
  ramp train2 45% 3s
label cross
  wait sensor after_bridge on timeout 1m
  unlock bridge
goto lap
)"},

    {"random_station",
     R"(# Loop with a two-track station. On every lap the train picks
# track 1 or 2 at random (turnout 1) and waits a variable time.
loco train dcc 3
sensor arrival 7         # sensor at the station entry
sensor platform 8        # sensor at the stopping point

power on
func train 0 on
dir train fwd
label lap
  ramp train 60% 3s
  wait sensor arrival on
  if chance 50 goto track2
  switch 1 straight
  log entering on track 1
  goto stop
label track2
  switch 1 round
  log entering on track 2
label stop
  ramp train 25% 1s
  wait sensor platform on timeout 1m
  speed train 0
  wait random 15s 45s
goto lap
)"},
};

static const int kDefaultScriptCount = sizeof(kDefaultScripts) / sizeof(kDefaultScripts[0]);
