// tca9548a.h — Multiplexor I2C de 8 canales.
#pragma once

#include <Arduino.h>
#include <Wire.h>

class Tca9548a {
 public:
  explicit Tca9548a(uint8_t addr) : addr_(addr) {}

  bool begin(TwoWire& wire);
  // Selecciona un canal (0..7). Si ya estaba seleccionado no toca el bus.
  bool select(uint8_t channel);
  // Desconecta todos los canales.
  bool disableAll();
  uint8_t current() const { return current_; }
  bool present() const { return present_; }

 private:
  bool write(uint8_t mask);
  TwoWire* wire_ = nullptr;
  uint8_t addr_;
  uint8_t current_ = 0xFF;  // 0xFF = desconocido
  bool present_ = false;
};
