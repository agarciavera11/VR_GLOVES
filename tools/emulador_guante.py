"""emulador_guante.py — Simula el firmware del guante por UDP.

Sirve para desarrollar y probar Unity (o guante_cli.py) sin el hardware:
responde PING/STATUS/ACK igual que el firmware y muestra en consola qué
haría cada dedo. Puede añadir retardo y pérdida artificiales.

    python emulador_guante.py                 # escucha en 0.0.0.0:8888
    python emulador_guante.py --retardo-ms 8 --perdida 0.02 --dedos 1,2,3
"""
from __future__ import annotations

import argparse
import random
import socket
import struct
import threading
import time

import protocolo as p


class Emulador:
    def __init__(self, host: str, port: int, announce_port: int, present_mask: int,
                 delay_ms: float = 0.0, loss: float = 0.0, verbose: bool = True,
                 announce: bool = True):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind((host, port))
        self.sock.settimeout(0.2)
        self.port = self.sock.getsockname()[1]
        self.announce_port = announce_port
        self.present = present_mask
        self.delay = delay_ms / 1000.0
        self.loss = loss
        self.verbose = verbose
        self.announce = announce
        self.t0 = time.monotonic()
        self.rx = 0
        self.err = 0
        self.host = None
        self.last_rx = 0.0
        self.log: list[str] = []
        self._stop = threading.Event()

    def micros(self) -> int:
        return int((time.monotonic() - self.t0) * 1e6) & 0xFFFFFFFF

    def status_payload(self) -> bytes:
        return p.encode_status(p.Status("0.4", 5, self.present, self.present, -55,
                                        int((time.monotonic() - self.t0) * 1000), self.rx,
                                        self.err))

    def _say(self, msg: str) -> None:
        self.log.append(msg)
        if self.verbose:
            print(msg, flush=True)

    def handle(self, data: bytes, addr) -> bytes | None:
        rx_us = self.micros()
        try:
            pkt = p.parse(data)
        except p.ProtocolError as e:
            self.err += 1
            code = {v: k for k, v in p.PARSE_ERRORS.items()}[str(e)]
            if code in (1, 2):  # TOO_SHORT / BAD_MAGIC: no se responde
                return None
            seq = struct.unpack_from("<H", data, 4)[0] if len(data) >= 6 else 0
            return p.build(p.ERROR, seq, bytes([code]))
        self.rx += 1
        self.host, self.last_rx = addr, time.monotonic()
        pl = pkt.payload
        proc = lambda: min(self.micros() - rx_us, 0xFFFF)  # noqa: E731
        if pkt.type == p.PING:
            return p.build(p.PONG, pkt.seq, pl[:4] + struct.pack("<IH", rx_us, proc()))
        if pkt.type == p.STATUS_REQ:
            return p.build(p.STATUS, pkt.seq, self.status_payload())
        if pkt.type == p.EFFECT:
            self._say(f"[efecto {pl[1]:3d}] {p.mask_to_names(pl[0] & self.present)}")
        elif pkt.type == p.PULSE:
            amp, dur = struct.unpack_from("<BH", pl, 1)
            self._say(f"[pulso amp={amp:3d} {dur:4d} ms] {p.mask_to_names(pl[0] & self.present)}")
        elif pkt.type == p.SEQUENCE:
            self._say(f"[secuencia {list(pl[1:])}] {p.mask_to_names(pl[0] & self.present)}")
        elif pkt.type == p.STOP:
            self._say(f"[stop] {p.mask_to_names(pl[0] & self.present)}")
        else:
            return None
        if pkt.flags & p.FLAG_ACK_REQUEST:
            return p.build(p.ACK, pkt.seq, struct.pack("<H", proc()))
        return None

    def serve(self) -> None:
        last_announce = 0.0
        seq = 0
        while not self._stop.is_set():
            now = time.monotonic()
            if self.host and now - self.last_rx > 3.0:
                self.host = None
            if self.announce and not self.host and now - last_announce >= 1.0:
                last_announce = now
                try:
                    self.sock.sendto(p.build(p.ANNOUNCE, seq, self.status_payload()),
                                     ("255.255.255.255", self.announce_port))
                except OSError:
                    pass
                seq = (seq + 1) & 0xFFFF
            try:
                data, addr = self.sock.recvfrom(256)
            except socket.timeout:
                continue
            except OSError:
                break
            if self.loss and random.random() < self.loss:
                continue
            reply = self.handle(data, addr)
            if self.delay:
                time.sleep(self.delay)
            if reply:
                self.sock.sendto(reply, addr)

    def start_thread(self) -> threading.Thread:
        t = threading.Thread(target=self.serve, daemon=True)
        t.start()
        return t

    def close(self) -> None:
        self._stop.set()
        self.sock.close()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--puerto", type=int, default=8888)
    ap.add_argument("--puerto-anuncio", type=int, default=8889)
    ap.add_argument("--dedos", default="a", help="dedos presentes, p. ej. 1,2,3 (por defecto todos)")
    ap.add_argument("--retardo-ms", type=float, default=0.0)
    ap.add_argument("--perdida", type=float, default=0.0, help="probabilidad de perder un paquete (0..1)")
    args = ap.parse_args()
    emu = Emulador(args.host, args.puerto, args.puerto_anuncio, p.parse_fingers(args.dedos),
                   args.retardo_ms, args.perdida)
    print(f"Emulador del guante en {args.host}:{emu.port} (dedos: {p.mask_to_names(emu.present)})")
    try:
        emu.serve()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
