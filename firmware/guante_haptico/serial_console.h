// serial_console.h — Consola de texto por USB para pruebas sin Unity.
//
// Mantiene los comandos de la tesis base (v<n>, v<n>a, v<n>b, scan) y añade
// comandos nuevos. Escribe "help" en el Monitor Serie (115200 baudios).
#pragma once

#include <Arduino.h>

#include "haptic_engine.h"
#include "net_link.h"
#include "tca9548a.h"

class SerialConsole {
 public:
  SerialConsole(Stream& io, HapticEngine& engine, Tca9548a& mux, NetLink& net)
      : io_(io), engine_(engine), mux_(mux), net_(net) {}

  void poll(uint32_t nowMs);
  void printHelp();

 private:
  void execute(char* line, uint32_t nowMs);
  void scan();
  void status();
  void bench();
  // Interpreta "0".."4" -> máscara de un dedo, "a" -> todos. 0 si no es válido.
  uint8_t parseFingerMask(const char* tok);

  Stream& io_;
  HapticEngine& engine_;
  Tca9548a& mux_;
  NetLink& net_;
  char buf_[48];
  uint8_t len_ = 0;
};
