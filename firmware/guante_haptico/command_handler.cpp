#include "command_handler.h"

#include <string.h>

using namespace proto;

static uint16_t elapsedUs16(uint32_t fromMicros) {
  const uint32_t d = micros() - fromMicros;
  return d > 0xFFFF ? 0xFFFF : (uint16_t)d;
}

size_t CommandHandler::ack(uint16_t seq, uint32_t rxMicros, uint8_t* out, size_t cap) {
  uint8_t p[2];
  putU16(p, elapsedUs16(rxMicros));
  return build(ACK, 0, seq, p, sizeof(p), out, cap);
}

size_t CommandHandler::buildStatus(uint8_t type, uint16_t seq, int8_t rssi, uint8_t* out,
                                   size_t cap) {
  StatusPayload s;
  s.fwMajor = FW_VERSION_MAJOR;
  s.fwMinor = FW_VERSION_MINOR;
  s.fingerCount = FINGER_COUNT;
  s.presentMask = engine_.presentMask();
  s.calibratedMask = engine_.calibratedMask();
  s.rssi = rssi;
  s.uptimeMs = millis();
  s.rxPackets = rxPackets_;
  s.rxErrors = rxErrors_;
  uint8_t p[STATUS_PAYLOAD_SIZE];
  encodeStatus(s, p);
  return build(type, 0, seq, p, sizeof(p), out, cap);
}

size_t CommandHandler::handle(const uint8_t* buf, size_t n, uint32_t rxMicros, int8_t rssi,
                              uint8_t* out, size_t cap) {
  Packet pkt;
  const ParseResult r = parse(buf, n, pkt);
  if (r != PARSE_OK) {
    rxErrors_++;
    // Solo se responde ERROR si al menos la cabecera es nuestra; así no se
    // contesta a tráfico ajeno que llegue al puerto.
    if (r == PARSE_BAD_MAGIC || r == PARSE_TOO_SHORT) return 0;
    const uint8_t code = (uint8_t)r;
    return build(ERROR, 0, n >= HEADER_SIZE ? getU16(&buf[4]) : 0, &code, 1, out, cap);
  }
  rxPackets_++;
  lastPacketMs_ = millis();
  const bool wantAck = (pkt.flags & FLAG_ACK_REQUEST) != 0;
  const uint8_t* p = pkt.payload;

  switch (pkt.type) {
    case PING: {
      uint8_t o[10];
      memcpy(o, p, 4);           // eco del timestamp del host
      putU32(&o[4], rxMicros);   // reloj del guante (para estimar desfase)
      putU16(&o[8], elapsedUs16(rxMicros));
      return build(PONG, 0, pkt.seq, o, sizeof(o), out, cap);
    }
    case EFFECT:
      engine_.playEffect(p[0], p[1]);
      break;
    case PULSE:
      engine_.pulse(p[0], p[1], getU16(&p[2]), millis());
      break;
    case SEQUENCE:
      engine_.playSequence(p[0], &p[1], (uint8_t)(pkt.len - 1));
      break;
    case STOP:
      engine_.stop(p[0]);
      break;
    case STATUS_REQ:
      return buildStatus(STATUS, pkt.seq, rssi, out, cap);
    default:
      // Tipos guante->host recibidos por error: se ignoran.
      return 0;
  }
  return wantAck ? ack(pkt.seq, rxMicros, out, cap) : 0;
}
