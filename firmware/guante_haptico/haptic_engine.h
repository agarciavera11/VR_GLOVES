// haptic_engine.h — Gestión de los 5 dedos: detección, calibración y temporización.
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "drv2605l.h"
#include "tca9548a.h"

class HapticEngine {
 public:
  HapticEngine(TwoWire& wire, Tca9548a& mux) : wire_(wire), mux_(mux) {}

  // Detecta los drivers presentes y, si está habilitado, los calibra en paralelo.
  void begin(Print* log);
  // Relanza la autocalibración (bloquea ~1,2 s).
  void calibrate(Print* log);

  // Todas las funciones reciben una máscara de dedos (bit i = dedo i).
  uint8_t playEffect(uint8_t mask, uint8_t effectId);
  uint8_t playSequence(uint8_t mask, const uint8_t* ids, uint8_t count);
  // amplitude 0..255 (se escala a 0..127 RTP); durationMs se limita a MAX_PULSE_MS
  uint8_t pulse(uint8_t mask, uint8_t amplitude, uint16_t durationMs, uint32_t nowMs);
  uint8_t stop(uint8_t mask);

  // Llamar en cada loop(): termina los pulsos RTP cuyo tiempo venció.
  void update(uint32_t nowMs);

  uint8_t presentMask() const { return presentMask_; }
  uint8_t calibratedMask() const { return calibratedMask_; }
  const Drv2605l& driver(uint8_t finger) const { return drivers_[finger]; }

 private:
  TwoWire& wire_;
  Tca9548a& mux_;
  Drv2605l drivers_[FINGER_COUNT];
  uint32_t pulseEndMs_[FINGER_COUNT] = {0};
  bool pulseActive_[FINGER_COUNT] = {false};
  uint8_t presentMask_ = 0;
  uint8_t calibratedMask_ = 0;
};
