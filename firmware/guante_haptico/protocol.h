// protocol.h — Protocolo binario Unity <-> guante (v1)
//
// Código puro C++ (sin dependencias de Arduino) para poder probarlo en el PC.
// La especificación completa está en docs/protocolo.md. Cualquier cambio aquí
// debe replicarse en unity/.../HapticProtocol.cs y en tools/protocolo.py.
//
// Formato de un paquete (little-endian):
//   [0] MAGIC (0xA7)
//   [1] VERSION (1)
//   [2] TYPE
//   [3] FLAGS  (bit0 = ACK_REQUEST)
//   [4] SEQ lo
//   [5] SEQ hi
//   [6 .. 6+N-1] PAYLOAD (N bytes, depende de TYPE)
//   [6+N] CRC-8 (polinomio 0x07, init 0x00) sobre los bytes [0 .. 6+N-1]
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace proto {

constexpr uint8_t MAGIC = 0xA7;
constexpr uint8_t VERSION = 1;
constexpr size_t HEADER_SIZE = 6;
constexpr size_t CRC_SIZE = 1;
constexpr size_t MAX_PAYLOAD = 24;
constexpr size_t MAX_PACKET = HEADER_SIZE + MAX_PAYLOAD + CRC_SIZE;
constexpr uint8_t MAX_SEQUENCE_EFFECTS = 8;  // ranuras de forma de onda del DRV2605L

constexpr uint8_t FLAG_ACK_REQUEST = 0x01;

enum Type : uint8_t {
  // Host -> guante
  PING = 0x01,        // payload: u32 eco
  EFFECT = 0x10,      // payload: u8 mask, u8 effectId (1..123, librería LRA)
  PULSE = 0x11,       // payload: u8 mask, u8 amplitud (0..255), u16 duración ms
  SEQUENCE = 0x12,    // payload: u8 mask, 1..8 × u8 effectId
  STOP = 0x13,        // payload: u8 mask
  STATUS_REQ = 0x20,  // payload: vacío
  // Guante -> host
  PONG = 0x81,        // payload: u32 eco, u32 micros del guante, u16 µs de proceso
  ACK = 0x82,         // payload: u16 µs de proceso (SEQ = SEQ del comando)
  STATUS = 0xA0,      // payload: ver StatusPayload
  ANNOUNCE = 0xA1,    // payload: igual que STATUS, enviado por broadcast
  ERROR = 0xEE,       // payload: u8 código de error (ParseResult o ErrorCode)
};

enum ParseResult : uint8_t {
  PARSE_OK = 0,
  PARSE_TOO_SHORT = 1,
  PARSE_BAD_MAGIC = 2,
  PARSE_BAD_VERSION = 3,
  PARSE_BAD_CRC = 4,
  PARSE_BAD_LENGTH = 5,
  PARSE_UNKNOWN_TYPE = 6,
};

struct Packet {
  uint8_t type = 0;
  uint8_t flags = 0;
  uint16_t seq = 0;
  uint8_t len = 0;
  uint8_t payload[MAX_PAYLOAD] = {0};
};

// Payload de STATUS/ANNOUNCE (16 bytes en el cable).
struct StatusPayload {
  uint8_t fwMajor;
  uint8_t fwMinor;
  uint8_t fingerCount;
  uint8_t presentMask;     // bit i = hay DRV2605L en el dedo i
  uint8_t calibratedMask;  // bit i = autocalibración LRA correcta
  int8_t rssi;             // dBm (0 si no hay Wi-Fi)
  uint32_t uptimeMs;
  uint32_t rxPackets;
  uint16_t rxErrors;
};
constexpr uint8_t STATUS_PAYLOAD_SIZE = 16;

uint8_t crc8(const uint8_t* data, size_t len);

// Valida y decodifica un paquete recibido.
ParseResult parse(const uint8_t* buf, size_t n, Packet& out);

// Construye un paquete. Devuelve el tamaño total o 0 si no cabe.
size_t build(uint8_t type, uint8_t flags, uint16_t seq, const uint8_t* payload,
             uint8_t len, uint8_t* out, size_t cap);

// Ayudas little-endian
inline void putU16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}
inline void putU32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}
inline uint16_t getU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t getU32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

void encodeStatus(const StatusPayload& s, uint8_t* out16);
bool decodeStatus(const uint8_t* in, uint8_t len, StatusPayload& s);

}  // namespace proto
