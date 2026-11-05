"""guante_cli.py — Herramienta de línea de comandos para probar el guante por UDP.

Ejemplos:
    python guante_cli.py descubrir                       # espera el ANNOUNCE del guante
    python guante_cli.py --ip 192.168.1.50 estado
    python guante_cli.py --ip 192.168.1.50 efecto indice 47
    python guante_cli.py --ip 192.168.1.50 pulso 1,2,3 200 150
    python guante_cli.py --ip 192.168.1.50 latencia -n 500 --csv latencia.csv
    python guante_cli.py --ip 192.168.1.50 latencia --comando   # RTT de PULSE con ACK

La latencia que se mide es el tiempo de ida y vuelta (RTT) PC -> guante -> PC.
La latencia de ida se estima como RTT/2 (supone simetría); el firmware además
informa cuántos µs tardó en procesar el paquete.
"""
from __future__ import annotations

import argparse
import csv
import socket
import statistics
import struct
import sys
import time

import protocolo as p


class Cliente:
    def __init__(self, ip: str, port: int = 8888, local_port: int = 8889, timeout: float = 0.5):
        self.addr = (ip, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", local_port))
        self.sock.settimeout(timeout)
        self.seq = 0

    def next_seq(self) -> int:
        self.seq = (self.seq + 1) & 0xFFFF
        return self.seq

    def send(self, data: bytes) -> None:
        self.sock.sendto(data, self.addr)

    def wait(self, want_type: int, seq: int) -> p.Packet | None:
        deadline = time.perf_counter() + self.sock.gettimeout()
        while time.perf_counter() < deadline:
            try:
                data, _ = self.sock.recvfrom(256)
            except socket.timeout:
                return None
            try:
                pkt = p.parse(data)
            except p.ProtocolError:
                continue
            if pkt.type == p.ERROR and pkt.seq == seq:
                raise p.ProtocolError(f"el guante respondió ERROR {p.PARSE_ERRORS.get(pkt.payload[0])}")
            if pkt.type == want_type and pkt.seq == seq:
                return pkt
        return None

    def status(self) -> p.Status | None:
        s = self.next_seq()
        self.send(p.status_req(s))
        pkt = self.wait(p.STATUS, s)
        return p.decode_status(pkt.payload) if pkt else None

    def ping(self) -> tuple[float, int] | None:
        """Devuelve (RTT en ms, µs de proceso en el guante) o None si se perdió."""
        s = self.next_seq()
        t0 = time.perf_counter_ns()
        self.send(p.ping(s, t0 // 1000))
        pkt = self.wait(p.PONG, s)
        if not pkt:
            return None
        rtt = (time.perf_counter_ns() - t0) / 1e6
        return rtt, p.decode_pong(pkt.payload)[2]

    def pulse_ack(self, mask: int, amp: int, ms: int) -> tuple[float, int] | None:
        s = self.next_seq()
        t0 = time.perf_counter_ns()
        self.send(p.pulse(s, mask, amp, ms, ack=True))
        pkt = self.wait(p.ACK, s)
        if not pkt:
            return None
        return (time.perf_counter_ns() - t0) / 1e6, struct.unpack("<H", pkt.payload)[0]


def percentile(data: list[float], q: float) -> float:
    if not data:
        return float("nan")
    s = sorted(data)
    k = (len(s) - 1) * q
    lo, hi = int(k), min(int(k) + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def medir_latencia(cli: Cliente, n: int, intervalo_ms: float, comando: bool, mask: int,
                   csv_path: str | None) -> dict:
    filas, rtts, procs, perdidos = [], [], [], 0
    for i in range(n):
        r = cli.pulse_ack(mask, 120, 20) if comando else cli.ping()
        if r is None:
            perdidos += 1
            filas.append((i, "", ""))
        else:
            rtts.append(r[0])
            procs.append(r[1])
            filas.append((i, f"{r[0]:.3f}", r[1]))
        time.sleep(intervalo_ms / 1000)
    if csv_path:
        with open(csv_path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["muestra", "rtt_ms", "proceso_us"])
            w.writerows(filas)
    res = {
        "n": n, "perdidos": perdidos,
        "media_ms": statistics.fmean(rtts) if rtts else float("nan"),
        "p50_ms": percentile(rtts, 0.5), "p95_ms": percentile(rtts, 0.95),
        "p99_ms": percentile(rtts, 0.99), "max_ms": max(rtts) if rtts else float("nan"),
        "desv_ms": statistics.pstdev(rtts) if len(rtts) > 1 else 0.0,
        "proceso_us_media": statistics.fmean(procs) if procs else float("nan"),
    }
    return res


def descubrir(local_port: int, timeout: float) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", local_port))
    sock.settimeout(timeout)
    print(f"Escuchando anuncios en el puerto {local_port} durante {timeout:.0f} s...")
    try:
        while True:
            data, addr = sock.recvfrom(256)
            try:
                pkt = p.parse(data)
            except p.ProtocolError:
                continue
            if pkt.type == p.ANNOUNCE:
                st = p.decode_status(pkt.payload)
                print(f"Guante en {addr[0]}: fw {st.fw}, dedos {p.mask_to_names(st.present_mask)}, RSSI {st.rssi} dBm")
                return
    except socket.timeout:
        print("No se recibió ningún anuncio. ¿Mismo Wi-Fi? ¿El router bloquea broadcast?")
        sys.exit(1)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ip", default="127.0.0.1")
    ap.add_argument("--puerto", type=int, default=8888)
    ap.add_argument("--puerto-local", type=int, default=8889)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("descubrir")
    sub.add_parser("estado")
    sub.add_parser("ping")
    e = sub.add_parser("efecto"); e.add_argument("dedos"); e.add_argument("id", type=int)
    pu = sub.add_parser("pulso"); pu.add_argument("dedos"); pu.add_argument("amp", type=int); pu.add_argument("ms", type=int)
    sq = sub.add_parser("secuencia"); sq.add_argument("dedos"); sq.add_argument("ids", type=int, nargs="+")
    st = sub.add_parser("stop"); st.add_argument("dedos", nargs="?", default="a")
    la = sub.add_parser("latencia")
    la.add_argument("-n", type=int, default=200)
    la.add_argument("--intervalo-ms", type=float, default=20)
    la.add_argument("--comando", action="store_true", help="mide PULSE+ACK en vez de PING")
    la.add_argument("--dedos", default="1")
    la.add_argument("--csv")
    args = ap.parse_args(argv)

    if args.cmd == "descubrir":
        descubrir(args.puerto_local, 5)
        return 0
    cli = Cliente(args.ip, args.puerto, args.puerto_local)
    if args.cmd == "estado":
        s = cli.status()
        if not s:
            print("Sin respuesta"); return 1
        print(f"fw {s.fw} | dedos presentes: {p.mask_to_names(s.present_mask)} | "
              f"calibrados: {p.mask_to_names(s.calibrated_mask)} | RSSI {s.rssi} dBm | "
              f"uptime {s.uptime_ms/1000:.1f} s | rx {s.rx_packets} | errores {s.rx_errors}")
    elif args.cmd == "ping":
        r = cli.ping()
        print("Sin respuesta" if r is None else f"RTT {r[0]:.2f} ms (proceso en guante {r[1]} µs)")
    elif args.cmd == "efecto":
        cli.send(p.effect(cli.next_seq(), p.parse_fingers(args.dedos), args.id))
    elif args.cmd == "pulso":
        cli.send(p.pulse(cli.next_seq(), p.parse_fingers(args.dedos), args.amp, args.ms))
    elif args.cmd == "secuencia":
        cli.send(p.sequence(cli.next_seq(), p.parse_fingers(args.dedos), args.ids))
    elif args.cmd == "stop":
        cli.send(p.stop(cli.next_seq(), p.parse_fingers(args.dedos)))
    elif args.cmd == "latencia":
        r = medir_latencia(cli, args.n, args.intervalo_ms, args.comando,
                           p.parse_fingers(args.dedos), args.csv)
        tipo = "PULSE+ACK" if args.comando else "PING"
        print(f"{tipo}: n={r['n']} perdidos={r['perdidos']}")
        print(f"  RTT media {r['media_ms']:.2f} ms | p50 {r['p50_ms']:.2f} | p95 {r['p95_ms']:.2f} | "
              f"p99 {r['p99_ms']:.2f} | max {r['max_ms']:.2f} | desv {r['desv_ms']:.2f}")
        print(f"  ida estimada (RTT/2) p50 {r['p50_ms']/2:.2f} ms | proceso en guante {r['proceso_us_media']:.0f} µs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
