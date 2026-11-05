#include "net_link.h"

#include "config.h"
#include "protocol.h"

bool NetLink::begin(Print* log) {
  if (WiFi.status() == WL_NO_MODULE) {
    if (log) log->println(F("[wifi] no se encuentra el modulo NINA"));
    return false;
  }
  if (log) {
    log->print(F("[wifi] firmware NINA "));
    log->println(WiFi.firmwareVersion());
  }
  return connect(log);
}

bool NetLink::connect(Print* log) {
  if (strlen(WIFI_SSID) == 0) {
    if (log) log->println(F("[wifi] sin SSID configurado (secrets.h); solo modo serie"));
    return false;
  }
  if (log) {
    log->print(F("[wifi] conectando a "));
    log->println(WIFI_SSID);
  }
  const uint32_t start = millis();
  int status = WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (status != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    status = WiFi.status();
  }
  connected_ = (status == WL_CONNECTED);
  if (!connected_) {
    if (log) log->println(F("[wifi] no se pudo conectar; se reintentara"));
    return false;
  }
  // El ahorro de energia del NINA anade decenas de ms de latencia en recepcion.
  WiFi.noLowPowerMode();
  udp_.stop();
  udp_.begin(UDP_LISTEN_PORT);
  hasHost_ = false;
  if (log) printInfo(*log);
  return true;
}

int8_t NetLink::rssi() {
  // WiFi.RSSI() consulta al NINA por SPI: se cachea para no frenar el loop.
  if (connected_ && millis() - lastRssiMs_ > 2000) {
    rssi_ = (int8_t)WiFi.RSSI();
    lastRssiMs_ = millis();
  }
  return connected_ ? rssi_ : 0;
}

void NetLink::printInfo(Print& out) {
  out.print(F("[wifi] estado="));
  out.print(connected_ ? F("conectado") : F("desconectado"));
  if (connected_) {
    out.print(F(" ip="));
    out.print(WiFi.localIP());
    out.print(F(" puerto="));
    out.print(UDP_LISTEN_PORT);
    out.print(F(" rssi="));
    out.print(WiFi.RSSI());
    out.print(F(" dBm"));
  }
  if (hasHost_) {
    out.print(F(" host="));
    out.print(hostIp_);
    out.print(':');
    out.print(hostPort_);
  }
  out.println();
}

void NetLink::send(const IPAddress& ip, uint16_t port, const uint8_t* data, size_t len) {
  udp_.beginPacket(ip, port);
  udp_.write(data, len);
  udp_.endPacket();
}

void NetLink::poll() {
  if (!connected_) return;
  // Se vacia la cola completa en cada vuelta para no acumular retraso.
  for (int guard = 0; guard < 8; guard++) {
    const int size = udp_.parsePacket();
    if (size <= 0) return;
    const uint32_t rxMicros = micros();
    uint8_t in[proto::MAX_PACKET + 4];
    const int n = udp_.read(in, sizeof(in));
    if (n <= 0) continue;
    hostIp_ = udp_.remoteIP();
    hostPort_ = udp_.remotePort();
    hasHost_ = true;
    uint8_t out[proto::MAX_PACKET];
    const size_t outLen = handler_.handle(in, (size_t)n, rxMicros, rssi(), out, sizeof(out));
    if (outLen > 0) send(hostIp_, hostPort_, out, outLen);
  }
}

void NetLink::maintain(uint32_t nowMs, Print* log) {
  if (connected_ && WiFi.status() != WL_CONNECTED) {
    connected_ = false;
    hasHost_ = false;
    if (log) log->println(F("[wifi] conexion perdida"));
  }
  if (!connected_) {
    if (nowMs - lastRetryMs_ >= WIFI_RETRY_MS) {
      lastRetryMs_ = nowMs;
      connect(log);
    }
    return;
  }
  // Si el host deja de hablar, se vuelve a anunciar para que Unity lo encuentre.
  if (hasHost_ && nowMs - handler_.lastPacketMs() > HOST_TIMEOUT_MS) hasHost_ = false;
  if (!hasHost_ && nowMs - lastAnnounceMs_ >= ANNOUNCE_INTERVAL_MS) {
    lastAnnounceMs_ = nowMs;
    uint8_t out[proto::MAX_PACKET];
    const size_t len = handler_.buildStatus(proto::ANNOUNCE, announceSeq_++, rssi(), out, sizeof(out));
    send(IPAddress(255, 255, 255, 255), UDP_ANNOUNCE_PORT, out, len);
  }
}
