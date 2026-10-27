#include "console.h"

#include <Arduino.h>

#include "app.h"
#include "event_log.h"
#include "script_store.h"

namespace {

String lineBuf;

void printHelp() {
  Serial.println(F(
      "\nConsole commands:\n"
      "  help                     this help\n"
      "  status                   status: track power, CAN, locos, sensors, scripts\n"
      "  scripts                  list of saved scripts\n"
      "  show <script>            show a script\n"
      "  run <script>             start a script\n"
      "  stop <script> | stop all stop scripts (and their locos)\n"
      "  !                        EMERGENCY STOP (cuts track power)\n"
      "  ping                     find devices on the CAN bus\n"
      "  monitor on|off           show/hide CAN frames\n"
      "  config                   show the configuration\n"
      "  restart                  restart the ESP32\n"
      "Immediate commands (as in scripts):\n"
      "  power on|off             speed mm2:24 50%      dir dcc:3 rev\n"
      "  func mfx:5 0 on          switch 5 straight     halt\n"));
}

void printStatus() {
  const char* power = controller.power() == marklin::PowerState::On    ? "ON"
                      : controller.power() == marklin::PowerState::Off ? "OFF"
                                                                       : "unknown";
  Serial.printf("Track power: %s%s\n", power, controller.overload() ? " (OVERLOAD)" : "");
  Serial.printf("CAN: %s, rx=%lu tx=%lu tx_fail=%lu TEC=%lu REC=%lu hash=0x%04X\n",
                canBus.stateName(), static_cast<unsigned long>(controller.framesRx()),
                static_cast<unsigned long>(controller.framesTx()),
                static_cast<unsigned long>(controller.txErrors()),
                static_cast<unsigned long>(canBus.txErrorCounter()),
                static_cast<unsigned long>(canBus.rxErrorCounter()), controller.hash());
  Serial.printf("MS2 detected: %s\n", controller.ms2Present() ? "yes" : "no");
  for (int i = 0; i < controller.deviceCount(); i++) {
    const marklin::DeviceInfo& d = controller.device(i);
    Serial.printf("  device %s uid 0x%08lX v%u.%u\n", marklin::deviceTypeName(d.type, d.uid),
                  static_cast<unsigned long>(d.uid), d.versionHigh, d.versionLow);
  }
  Serial.println("Locos:");
  for (int i = 0; i < controller.locoCount(); i++) {
    const marklin::LocoState& l = controller.loco(i);
    Serial.printf("  %-16s speed %4u  dir %s  functions 0x%08lX\n", locoLabel(l.uid).c_str(),
                  l.speed, l.direction == 1 ? "forward" : l.direction == 2 ? "reverse" : "?",
                  static_cast<unsigned long>(l.functions));
  }
  Serial.println("Sensors:");
  for (int i = 0; i < controller.sensorCount(); i++) {
    const marklin::SensorState& s = controller.sensor(i);
    Serial.printf("  %4lu %-14s %s\n", static_cast<unsigned long>(s.id), sensorName(s.id).c_str(),
                  s.state == 1 ? "ON" : s.state == 0 ? "off" : "?");
  }
  Serial.println("Scripts:");
  const uint32_t now = millis();
  for (int i = 0; i < automation::Engine::kMaxRunners; i++) {
    const automation::Runner& r = engine.runner(i);
    if (r.state() == automation::RunState::Idle) continue;
    Serial.printf("  [%d] %-16s %-10s line %d: %s\n", i, r.name().c_str(),
                  automation::runStateName(r.state()), r.line(), r.activity(now).c_str());
  }
}

void process(String line) {
  line.trim();
  if (line.isEmpty()) return;
  String cmd = line;
  String arg;
  const int sp = line.indexOf(' ');
  if (sp > 0) {
    cmd = line.substring(0, sp);
    arg = line.substring(sp + 1);
    arg.trim();
  }
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "ayuda" || cmd == "?") {
    printHelp();
  } else if (cmd == "status" || cmd == "estado") {
    printStatus();
  } else if (cmd == "scripts") {
    for (const String& n : scripts::list())
      Serial.printf("  %s%s\n", n.c_str(), engine.isRunning(n.c_str()) ? "  (running)" : "");
  } else if (cmd == "show") {
    String src;
    if (scripts::read(arg, &src))
      Serial.println(src);
    else
      Serial.println("that script does not exist");
  } else if (cmd == "run") {
    String err;
    if (!startScript(arg, &err)) Serial.printf("error: %s\n", err.c_str());
  } else if (cmd == "stop") {
    if (arg == "all" || arg == "todos")
      engine.stopAll();
    else if (!engine.stopByName(arg.c_str()))
      Serial.println("that script is not running");
  } else if (cmd == "!" || cmd == "emergencia") {
    emergencyStop("serial console");
  } else if (cmd == "ping") {
    controller.ping();
  } else if (cmd == "monitor") {
    eventLog.echoCan = arg != "off";
    Serial.printf("monitor CAN %s\n", eventLog.echoCan ? "on" : "off");
  } else if (cmd == "config") {
    Serial.println(configToJson(config));
  } else if (cmd == "restart" || cmd == "reiniciar") {
    ESP.restart();
  } else {
    const std::string err = engine.exec(line.c_str());
    if (!err.empty()) Serial.printf("error: %s  (type 'help')\n", err.c_str());
  }
}

}  // namespace

void consoleSetup() {
  lineBuf.reserve(128);
  Serial.println(F("Type 'help' to see the console commands."));
}

void consoleLoop() {
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      process(lineBuf);
      lineBuf = "";
    } else if (lineBuf.length() < 200) {
      lineBuf += ch;
    }
  }
}
