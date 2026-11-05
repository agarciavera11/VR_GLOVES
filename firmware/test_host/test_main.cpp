// Pruebas del firmware en el PC (sin hardware), con bus I2C y Wi-Fi simulados.
// Ejecutar:  make -C firmware/test_host
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Arduino.h"
#include "Wire.h"
#include "WiFiNINA.h"
#include "../guante_haptico/command_handler.h"
#include "../guante_haptico/haptic_engine.h"
#include "../guante_haptico/net_link.h"
#include "../guante_haptico/protocol.h"
#include "../guante_haptico/serial_console.h"
#include "../guante_haptico/tca9548a.h"

uint32_t g_millis = 1000, g_micros = 1000000;
TwoWire Wire;
FakeWiFi WiFi;

static int failures = 0, checks = 0;
#define CHECK(cond)                                                     \
  do {                                                                  \
    checks++;                                                           \
    if (!(cond)) {                                                      \
      failures++;                                                       \
      printf("  FALLO %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                   \
  } while (0)

using namespace proto;

static std::vector<uint8_t> pkt(uint8_t type, uint8_t flags, uint16_t seq,
                                std::vector<uint8_t> payload) {
  std::vector<uint8_t> out(MAX_PACKET);
  size_t n = build(type, flags, seq, payload.data(), (uint8_t)payload.size(), out.data(), out.size());
  out.resize(n);
  return out;
}

static std::string hex(const std::vector<uint8_t>& v) {
  std::string s;
  char b[4];
  for (uint8_t x : v) {
    snprintf(b, sizeof b, "%02X", x);
    s += b;
  }
  return s;
}

static void testProtocol() {
  puts("[protocolo]");
  const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(crc8(check, 9) == 0xF4);  // valor de referencia CRC-8/SMBUS

  auto p = pkt(PULSE, FLAG_ACK_REQUEST, 0x1234, {0x02, 200, 0x50, 0x00});
  CHECK(p.size() == HEADER_SIZE + 4 + 1);
  Packet out;
  CHECK(parse(p.data(), p.size(), out) == PARSE_OK);
  CHECK(out.type == PULSE && out.seq == 0x1234 && out.flags == 1 && out.len == 4);
  CHECK(getU16(&out.payload[2]) == 80);

  auto bad = p;
  bad.back() ^= 0xFF;
  CHECK(parse(bad.data(), bad.size(), out) == PARSE_BAD_CRC);
  bad = p;
  bad[0] = 0x00;
  CHECK(parse(bad.data(), bad.size(), out) == PARSE_BAD_MAGIC);
  bad = p;
  bad[1] = 9;
  CHECK(parse(bad.data(), bad.size(), out) == PARSE_BAD_VERSION);
  CHECK(parse(p.data(), 3, out) == PARSE_TOO_SHORT);
  auto wrongLen = pkt(EFFECT, 0, 1, {0x01});
  CHECK(parse(wrongLen.data(), wrongLen.size(), out) == PARSE_BAD_LENGTH);
  auto unknown = pkt(0x55, 0, 1, {});
  CHECK(parse(unknown.data(), unknown.size(), out) == PARSE_UNKNOWN_TYPE);
  auto seqTooLong = pkt(SEQUENCE, 0, 1, {1, 1, 2, 3, 4, 5, 6, 7, 8, 9});
  CHECK(parse(seqTooLong.data(), seqTooLong.size(), out) == PARSE_BAD_LENGTH);

  StatusPayload s{0, 4, 5, 0x0E, 0x0C, -60, 123456, 42, 3}, d{};
  uint8_t buf[16];
  encodeStatus(s, buf);
  CHECK(decodeStatus(buf, 16, d));
  CHECK(d.rssi == -60 && d.uptimeMs == 123456 && d.rxPackets == 42 && d.presentMask == 0x0E);
}

// Vectores de referencia: los mismos bytes se comprueban en Python y en C#.
static void writeGoldenVectors() {
  FILE* f = fopen("golden_vectors.txt", "w");
  if (!f) return;
  fprintf(f, "# tipo seq flags payload_hex => paquete_hex (generado por test_host)\n");
  struct V { const char* name; uint8_t t, fl; uint16_t seq; std::vector<uint8_t> p; } vs[] = {
      {"PING", PING, 0, 1, {0x78, 0x56, 0x34, 0x12}},
      {"EFFECT", EFFECT, 0, 2, {0x02, 47}},
      {"PULSE", PULSE, FLAG_ACK_REQUEST, 0x0102, {0x1F, 255, 0x2C, 0x01}},
      {"SEQUENCE", SEQUENCE, 0, 3, {0x0E, 1, 10, 52}},
      {"STOP", STOP, 0, 65535, {0xFF}},
      {"STATUS_REQ", STATUS_REQ, 0, 7, {}},
  };
  for (auto& v : vs) {
    auto b = pkt(v.t, v.fl, v.seq, v.p);
    fprintf(f, "%s %u %u %s %s\n", v.name, v.seq, v.fl, v.p.empty() ? "-" : hex(v.p).c_str(),
            hex(b).c_str());
  }
  fclose(f);
}

struct Rig {
  Tca9548a mux{0x70};
  HapticEngine engine{Wire, mux};
  CommandHandler handler{engine};
  Rig(std::vector<int> channels) {
    Wire = TwoWire();
    for (int c : channels) Wire.channelPopulated[c] = true;
    mux.begin(Wire);
  }
};

static void testEngine() {
  puts("[motor haptico]");
  Rig r({1, 2, 3});  // configuración de la tesis base: índice, medio, anular
  Print log;
  r.engine.begin(&log);
  CHECK(r.engine.presentMask() == 0b01110);
  CHECK(r.engine.calibratedMask() == 0b01110);
  FakeDrv& idx = Wire.drv[1];
  CHECK(idx.reg[drv::REG_FEEDBACK] == 0xB6);       // LRA
  CHECK(idx.reg[drv::REG_LIBRARY] == 6);           // librería LRA
  CHECK((idx.reg[drv::REG_MODE] & 0x40) == 0);     // fuera de standby
  CHECK(idx.reg[drv::REG_RATED_VOLTAGE] >= 60 && idx.reg[drv::REG_RATED_VOLTAGE] <= 80);
  printf("  RATED_VOLTAGE=%u OD_CLAMP=%u CONTROL1=0x%02X\n", idx.reg[0x16], idx.reg[0x17], idx.reg[0x1B]);

  // Efecto en el índice
  CHECK(r.engine.playEffect(1u << 1, 47) == (1u << 1));
  CHECK(idx.reg[drv::REG_WAVESEQ1] == 47 && idx.reg[drv::REG_WAVESEQ1 + 1] == 0);
  CHECK(idx.reg[drv::REG_GO] == 1 && (idx.reg[drv::REG_MODE] & 7) == drv::MODE_INTTRIG);

  // El pulgar no existe: se ignora sin error
  CHECK(r.engine.playEffect(1u << 0, 1) == 0);

  // Pulso RTP y fin automático
  const uint32_t before = Wire.transactions;
  CHECK(r.engine.pulse(1u << 2, 255, 100, millis()) == (1u << 2));
  const uint32_t txFirst = Wire.transactions - before;
  CHECK(Wire.drv[2].reg[drv::REG_RTP_INPUT] == 127);
  CHECK((Wire.drv[2].reg[drv::REG_MODE] & 7) == drv::MODE_RTP);
  r.engine.update(millis() + 99);
  CHECK(Wire.drv[2].reg[drv::REG_RTP_INPUT] == 127);
  r.engine.update(millis() + 100);
  CHECK(Wire.drv[2].reg[drv::REG_RTP_INPUT] == 0);
  // Segundo pulso en el mismo dedo: canal y modo cacheados => 1 sola escritura
  const uint32_t b2 = Wire.transactions;
  r.engine.pulse(1u << 2, 128, 50, millis());
  const uint32_t txSecond = Wire.transactions - b2;
  printf("  transacciones I2C por pulso: primero=%u, repetido=%u\n", txFirst, txSecond);
  CHECK(txSecond == 1);

  // Duración limitada por seguridad
  r.engine.pulse(1u << 3, 255, 60000, millis());
  r.engine.update(millis() + MAX_PULSE_MS);
  CHECK(Wire.drv[3].reg[drv::REG_RTP_INPUT] == 0);

  // Secuencia completa de 8 efectos (sin terminador)
  const uint8_t ids[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  r.engine.playSequence(1u << 1, ids, 8);
  CHECK(idx.reg[drv::REG_WAVESEQ1 + 7] == 8);
}

static void testHandler() {
  puts("[manejador de comandos]");
  Rig r({1, 2, 3});
  Print log;
  r.engine.begin(&log);
  uint8_t out[MAX_PACKET];
  Packet resp;

  auto ping = pkt(PING, 0, 10, {0xEF, 0xBE, 0xAD, 0xDE});
  size_t n = r.handler.handle(ping.data(), ping.size(), micros(), -50, out, sizeof out);
  CHECK(n > 0 && parse(out, n, resp) == PARSE_OK);
  CHECK(resp.type == PONG && resp.seq == 10 && getU32(resp.payload) == 0xDEADBEEF);

  auto eff = pkt(EFFECT, 0, 11, {0b00110, 52});
  n = r.handler.handle(eff.data(), eff.size(), micros(), -50, out, sizeof out);
  CHECK(n == 0);  // sin ACK pedido no hay respuesta (mínima latencia)
  CHECK(Wire.drv[1].reg[drv::REG_WAVESEQ1] == 52 && Wire.drv[2].reg[drv::REG_WAVESEQ1] == 52);

  auto pul = pkt(PULSE, FLAG_ACK_REQUEST, 12, {0b01000, 100, 30, 0});
  n = r.handler.handle(pul.data(), pul.size(), micros(), -50, out, sizeof out);
  CHECK(n > 0 && parse(out, n, resp) == PARSE_OK && resp.type == ACK && resp.seq == 12);
  CHECK(Wire.drv[3].reg[drv::REG_RTP_INPUT] == 50);

  auto st = pkt(STATUS_REQ, 0, 13, {});
  n = r.handler.handle(st.data(), st.size(), micros(), -61, out, sizeof out);
  StatusPayload s{};
  CHECK(n > 0 && parse(out, n, resp) == PARSE_OK && resp.type == STATUS);
  CHECK(decodeStatus(resp.payload, resp.len, s) && s.presentMask == 0b01110 && s.rssi == -61);
  CHECK(s.rxPackets == 4 && s.fingerCount == 5);

  auto bad = pul;
  bad.back() ^= 1;
  n = r.handler.handle(bad.data(), bad.size(), micros(), 0, out, sizeof out);
  CHECK(n > 0 && parse(out, n, resp) == PARSE_OK && resp.type == ERROR && resp.payload[0] == PARSE_BAD_CRC);
  CHECK(r.handler.rxErrors() == 1);
  // Tráfico ajeno (otro magic) no recibe respuesta
  const uint8_t junk[] = {'h', 'o', 'l', 'a', '!', '!', '!'};
  CHECK(r.handler.handle(junk, sizeof junk, micros(), 0, out, sizeof out) == 0);
}

static void testNet() {
  puts("[red UDP]");
  Rig r({1});
  Print log;
  r.engine.begin(&log);
  NetLink net(r.handler);
  CHECK(net.begin(&log) || std::string(WIFI_SSID).empty());
  if (!net.connected()) {
    puts("  (sin secrets.h: se omite)");
    return;
  }
  CHECK(!WiFi.lowPower);  // ahorro de energía desactivado
  WiFiUDP& udp = net.udpForTest();
  CHECK(udp.port == 8888);

  // Sin host: se anuncia por broadcast al puerto 8889
  net.maintain(millis() + 2000, &log);
  CHECK(udp.sent.size() == 1 && udp.sent[0].port == 8889 && udp.sent[0].ip.b[0] == 255);
  Packet resp;
  CHECK(parse(udp.sent[0].data.data(), udp.sent[0].data.size(), resp) == PARSE_OK && resp.type == ANNOUNCE);

  // Llega un PING desde Unity: respuesta al remitente
  auto ping = pkt(PING, 0, 5, {1, 2, 3, 4});
  udp.inbox.push_back(Datagram{ping, IPAddress(192, 168, 1, 20), 8889});
  udp.inbox.push_back(Datagram{pkt(EFFECT, 0, 6, {0x02, 1}), IPAddress(192, 168, 1, 20), 8889});
  net.poll();
  CHECK(udp.sent.size() == 2 && udp.sent[1].ip.b[3] == 20 && udp.sent[1].port == 8889);
  CHECK(net.hasHost());
  CHECK(udp.inbox.empty());  // se vacía toda la cola en una vuelta
  // Con host activo ya no se anuncia
  net.maintain(millis() + 100, &log);
  CHECK(udp.sent.size() == 2);
}

static void testConsole() {
  puts("[consola serie]");
  Rig r({1, 2, 3});
  Print log;
  r.engine.begin(&log);
  NetLink net(r.handler);
  Stream io;
  SerialConsole console(io, r.engine, r.mux, net);
  io.in = "v1b\n";
  console.poll(millis());
  CHECK(Wire.drv[1].reg[drv::REG_WAVESEQ1] == 10);
  CHECK(io.out.find("OK") != std::string::npos);
  io.out.clear();
  io.in = "v0\n";
  console.poll(millis());
  CHECK(io.out.find("ERR") != std::string::npos);
  io.out.clear();
  io.in = "p a 255 100\r\n";
  console.poll(millis());
  CHECK(Wire.drv[3].reg[drv::REG_RTP_INPUT] == 127);
  CHECK(io.out.find("0b1110") != std::string::npos);
  io.out.clear();
  io.in = "scan\n";
  console.poll(millis());
  CHECK(io.out.find("canal 2: 0x5A") != std::string::npos);
  CHECK(io.out.find("canal 0: (vacio)") != std::string::npos);
  io.out.clear();
  io.in = "xyz\n";
  console.poll(millis());
  CHECK(io.out.find("desconocido") != std::string::npos);
}

int main() {
  testProtocol();
  writeGoldenVectors();
  testEngine();
  testHandler();
  testNet();
  testConsole();
  printf("\n%d comprobaciones, %d fallos\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
