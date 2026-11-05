// net_link.h — Wi-Fi (NINA-W102) + UDP. Recibe comandos y envía respuestas/anuncios.
#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

#include "command_handler.h"

class NetLink {
 public:
  explicit NetLink(CommandHandler& handler) : handler_(handler) {}

  // Intenta conectar (bloquea hasta WIFI_CONNECT_TIMEOUT_MS). Devuelve true si hay red.
  bool begin(Print* log);
  // Llamar en cada loop(): atiende todos los datagramas pendientes.
  void poll();
  // Reconexión y anuncios periódicos.
  void maintain(uint32_t nowMs, Print* log);

  bool connected() const { return connected_; }
  bool hasHost() const { return hasHost_; }
  int8_t rssi();
  void printInfo(Print& out);
#ifdef HOST_TEST
  WiFiUDP& udpForTest() { return udp_; }
#endif

 private:
  bool connect(Print* log);
  void send(const IPAddress& ip, uint16_t port, const uint8_t* data, size_t len);

  CommandHandler& handler_;
  WiFiUDP udp_;
  bool connected_ = false;
  bool hasHost_ = false;
  IPAddress hostIp_;
  uint16_t hostPort_ = 0;
  uint32_t lastAnnounceMs_ = 0;
  uint32_t lastRetryMs_ = 0;
  uint32_t lastRssiMs_ = 0;
  int8_t rssi_ = 0;
  uint16_t announceSeq_ = 0;
};
