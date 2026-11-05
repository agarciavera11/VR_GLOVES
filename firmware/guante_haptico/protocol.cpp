#include "protocol.h"

#include <string.h>

namespace proto {

uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

// Longitud válida del payload por tipo. Devuelve false si el tipo no se conoce.
static bool lengthIsValid(uint8_t type, uint8_t len) {
  switch (type) {
    case PING:       return len == 4;
    case EFFECT:     return len == 2;
    case PULSE:      return len == 4;
    case SEQUENCE:   return len >= 2 && len <= 1 + MAX_SEQUENCE_EFFECTS;
    case STOP:       return len == 1;
    case STATUS_REQ: return len == 0;
    case PONG:       return len == 10;
    case ACK:        return len == 2;
    case STATUS:
    case ANNOUNCE:   return len == STATUS_PAYLOAD_SIZE;
    case ERROR:      return len == 1;
    default:         return false;
  }
}

static bool typeIsKnown(uint8_t type) {
  switch (type) {
    case PING: case EFFECT: case PULSE: case SEQUENCE: case STOP:
    case STATUS_REQ: case PONG: case ACK: case STATUS: case ANNOUNCE:
    case ERROR:
      return true;
    default:
      return false;
  }
}

ParseResult parse(const uint8_t* buf, size_t n, Packet& out) {
  if (buf == nullptr || n < HEADER_SIZE + CRC_SIZE) return PARSE_TOO_SHORT;
  if (buf[0] != MAGIC) return PARSE_BAD_MAGIC;
  if (buf[1] != VERSION) return PARSE_BAD_VERSION;
  if (n > MAX_PACKET) return PARSE_BAD_LENGTH;
  if (crc8(buf, n - 1) != buf[n - 1]) return PARSE_BAD_CRC;

  const uint8_t type = buf[2];
  const uint8_t len = (uint8_t)(n - HEADER_SIZE - CRC_SIZE);
  if (!typeIsKnown(type)) return PARSE_UNKNOWN_TYPE;
  if (!lengthIsValid(type, len)) return PARSE_BAD_LENGTH;

  out.type = type;
  out.flags = buf[3];
  out.seq = getU16(&buf[4]);
  out.len = len;
  memcpy(out.payload, &buf[HEADER_SIZE], len);
  return PARSE_OK;
}

size_t build(uint8_t type, uint8_t flags, uint16_t seq, const uint8_t* payload,
             uint8_t len, uint8_t* out, size_t cap) {
  const size_t total = HEADER_SIZE + len + CRC_SIZE;
  if (len > MAX_PAYLOAD || total > cap || out == nullptr) return 0;
  out[0] = MAGIC;
  out[1] = VERSION;
  out[2] = type;
  out[3] = flags;
  putU16(&out[4], seq);
  if (len > 0) memcpy(&out[HEADER_SIZE], payload, len);
  out[total - 1] = crc8(out, total - 1);
  return total;
}

void encodeStatus(const StatusPayload& s, uint8_t* o) {
  o[0] = s.fwMajor;
  o[1] = s.fwMinor;
  o[2] = s.fingerCount;
  o[3] = s.presentMask;
  o[4] = s.calibratedMask;
  o[5] = (uint8_t)s.rssi;
  putU32(&o[6], s.uptimeMs);
  putU32(&o[10], s.rxPackets);
  putU16(&o[14], s.rxErrors);
}

bool decodeStatus(const uint8_t* in, uint8_t len, StatusPayload& s) {
  if (len != STATUS_PAYLOAD_SIZE) return false;
  s.fwMajor = in[0];
  s.fwMinor = in[1];
  s.fingerCount = in[2];
  s.presentMask = in[3];
  s.calibratedMask = in[4];
  s.rssi = (int8_t)in[5];
  s.uptimeMs = getU32(&in[6]);
  s.rxPackets = getU32(&in[10]);
  s.rxErrors = getU16(&in[14]);
  return true;
}

}  // namespace proto
