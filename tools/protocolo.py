"""protocolo.py — Implementación en Python del protocolo del guante (v1).

Espejo de firmware/guante_haptico/protocol.{h,cpp} y de HapticProtocol.cs.
Especificación: docs/protocolo.md
"""
from __future__ import annotations

import struct
from dataclasses import dataclass

MAGIC = 0xA7
VERSION = 1
HEADER_SIZE = 6
MAX_PAYLOAD = 24
MAX_PACKET = HEADER_SIZE + MAX_PAYLOAD + 1
FLAG_ACK_REQUEST = 0x01

PING, EFFECT, PULSE, SEQUENCE, STOP, STATUS_REQ = 0x01, 0x10, 0x11, 0x12, 0x13, 0x20
PONG, ACK, STATUS, ANNOUNCE, ERROR = 0x81, 0x82, 0xA0, 0xA1, 0xEE

TYPE_NAMES = {
    PING: "PING", EFFECT: "EFFECT", PULSE: "PULSE", SEQUENCE: "SEQUENCE", STOP: "STOP",
    STATUS_REQ: "STATUS_REQ", PONG: "PONG", ACK: "ACK", STATUS: "STATUS",
    ANNOUNCE: "ANNOUNCE", ERROR: "ERROR",
}

PARSE_ERRORS = {1: "TOO_SHORT", 2: "BAD_MAGIC", 3: "BAD_VERSION", 4: "BAD_CRC",
                5: "BAD_LENGTH", 6: "UNKNOWN_TYPE"}

FINGERS = ["pulgar", "indice", "medio", "anular", "menique"]
ALL_FINGERS = 0b11111


def _len_ok(t: int, n: int) -> bool:
    return {
        PING: n == 4, EFFECT: n == 2, PULSE: n == 4, SEQUENCE: 2 <= n <= 9, STOP: n == 1,
        STATUS_REQ: n == 0, PONG: n == 10, ACK: n == 2, STATUS: n == 16, ANNOUNCE: n == 16,
        ERROR: n == 1,
    }.get(t, False)


def crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


class ProtocolError(ValueError):
    pass


@dataclass
class Packet:
    type: int
    seq: int
    payload: bytes
    flags: int = 0

    @property
    def name(self) -> str:
        return TYPE_NAMES.get(self.type, hex(self.type))


def build(ptype: int, seq: int, payload: bytes = b"", flags: int = 0) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ProtocolError("payload demasiado grande")
    body = bytes([MAGIC, VERSION, ptype, flags]) + struct.pack("<H", seq & 0xFFFF) + payload
    return body + bytes([crc8(body)])


def parse(buf: bytes) -> Packet:
    if len(buf) < HEADER_SIZE + 1:
        raise ProtocolError("TOO_SHORT")
    if buf[0] != MAGIC:
        raise ProtocolError("BAD_MAGIC")
    if buf[1] != VERSION:
        raise ProtocolError("BAD_VERSION")
    if len(buf) > MAX_PACKET:
        raise ProtocolError("BAD_LENGTH")
    if crc8(buf[:-1]) != buf[-1]:
        raise ProtocolError("BAD_CRC")
    t = buf[2]
    payload = bytes(buf[HEADER_SIZE:-1])
    if t not in TYPE_NAMES:
        raise ProtocolError("UNKNOWN_TYPE")
    if not _len_ok(t, len(payload)):
        raise ProtocolError("BAD_LENGTH")
    return Packet(t, struct.unpack_from("<H", buf, 4)[0], payload, buf[3])


# --- Constructores de comandos ---------------------------------------------------
def ping(seq: int, echo: int) -> bytes:
    return build(PING, seq, struct.pack("<I", echo & 0xFFFFFFFF))


def effect(seq: int, mask: int, effect_id: int, ack: bool = False) -> bytes:
    return build(EFFECT, seq, bytes([mask, effect_id]), FLAG_ACK_REQUEST if ack else 0)


def pulse(seq: int, mask: int, amplitude: int, duration_ms: int, ack: bool = False) -> bytes:
    return build(PULSE, seq, struct.pack("<BBH", mask, amplitude, duration_ms),
                 FLAG_ACK_REQUEST if ack else 0)


def sequence(seq: int, mask: int, ids: list[int], ack: bool = False) -> bytes:
    if not 1 <= len(ids) <= 8:
        raise ProtocolError("la secuencia debe tener 1..8 efectos")
    return build(SEQUENCE, seq, bytes([mask, *ids]), FLAG_ACK_REQUEST if ack else 0)


def stop(seq: int, mask: int = ALL_FINGERS) -> bytes:
    return build(STOP, seq, bytes([mask]))


def status_req(seq: int) -> bytes:
    return build(STATUS_REQ, seq)


# --- Decodificadores de respuestas -----------------------------------------------
@dataclass
class Status:
    fw: str
    finger_count: int
    present_mask: int
    calibrated_mask: int
    rssi: int
    uptime_ms: int
    rx_packets: int
    rx_errors: int


def encode_status(s: Status) -> bytes:
    major, minor = (int(x) for x in s.fw.split("."))
    return struct.pack("<BBBBBbIIH", major, minor, s.finger_count, s.present_mask,
                       s.calibrated_mask, s.rssi, s.uptime_ms, s.rx_packets, s.rx_errors)


def decode_status(payload: bytes) -> Status:
    ma, mi, fc, pm, cm, rssi, up, rx, err = struct.unpack("<BBBBBbIIH", payload)
    return Status(f"{ma}.{mi}", fc, pm, cm, rssi, up, rx, err)


def decode_pong(payload: bytes) -> tuple[int, int, int]:
    """Devuelve (eco, micros_del_guante, us_de_proceso)."""
    return struct.unpack("<IIH", payload)


def mask_to_names(mask: int) -> str:
    return ", ".join(n for i, n in enumerate(FINGERS) if mask & (1 << i)) or "ninguno"


def parse_fingers(text: str) -> int:
    """'a' => todos; '1,2,3' o 'indice,medio' => máscara."""
    text = text.strip().lower()
    if text in ("a", "all", "todos"):
        return ALL_FINGERS
    mask = 0
    for tok in text.split(","):
        tok = tok.strip()
        if tok.isdigit():
            i = int(tok)
        elif tok in FINGERS:
            i = FINGERS.index(tok)
        else:
            raise ValueError(f"dedo desconocido: {tok}")
        if not 0 <= i < 5:
            raise ValueError(f"dedo fuera de rango: {tok}")
        mask |= 1 << i
    return mask
