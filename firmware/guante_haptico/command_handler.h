// command_handler.h — Traduce paquetes del protocolo a acciones del HapticEngine.
// Es independiente del transporte: lo usan tanto UDP como el puerto serie.
#pragma once

#include <Arduino.h>

#include "haptic_engine.h"
#include "protocol.h"

class CommandHandler {
 public:
  explicit CommandHandler(HapticEngine& engine) : engine_(engine) {}

  // Procesa un datagrama. rxMicros = micros() en el instante de recepción.
  // Escribe la respuesta (si la hay) en out y devuelve su tamaño (0 = no responder).
  size_t handle(const uint8_t* buf, size_t n, uint32_t rxMicros, int8_t rssi, uint8_t* out,
                size_t cap);

  size_t buildStatus(uint8_t type, uint16_t seq, int8_t rssi, uint8_t* out, size_t cap);

  uint32_t rxPackets() const { return rxPackets_; }
  uint16_t rxErrors() const { return rxErrors_; }
  uint32_t lastPacketMs() const { return lastPacketMs_; }

 private:
  size_t ack(uint16_t seq, uint32_t rxMicros, uint8_t* out, size_t cap);

  HapticEngine& engine_;
  uint32_t rxPackets_ = 0;
  uint16_t rxErrors_ = 0;
  uint32_t lastPacketMs_ = 0;
  uint16_t txSeq_ = 0;
};
