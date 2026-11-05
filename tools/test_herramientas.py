"""Pruebas de protocolo.py contra los vectores del firmware y del emulador.

    python -m pytest tools/test_herramientas.py   (o: python tools/test_herramientas.py)
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
import protocolo as p  # noqa: E402
from emulador_guante import Emulador  # noqa: E402
from guante_cli import Cliente, medir_latencia  # noqa: E402

GOLDEN = os.path.join(os.path.dirname(__file__), "..", "docs", "golden_vectors.txt")


def test_crc_referencia():
    assert p.crc8(b"123456789") == 0xF4


def test_vectores_del_firmware():
    n = 0
    for line in open(GOLDEN):
        if line.startswith("#") or not line.strip():
            continue
        name, seq, flags, payload, expected = line.split()
        payload = b"" if payload == "-" else bytes.fromhex(payload)
        t = {v: k for k, v in p.TYPE_NAMES.items()}[name]
        assert p.build(t, int(seq), payload, int(flags)).hex().upper() == expected
        pkt = p.parse(bytes.fromhex(expected))
        assert pkt.type == t and pkt.seq == int(seq) and pkt.payload == payload
        n += 1
    assert n == 6


def test_errores():
    good = p.pulse(1, 2, 200, 80)
    for bad, err in [(good[:-1] + bytes([good[-1] ^ 1]), "BAD_CRC"), (b"\x00" + good[1:], "BAD_MAGIC"),
                     (good[:3], "TOO_SHORT"), (p.build(p.EFFECT, 1, b"\x01"), "BAD_LENGTH")]:
        try:
            p.parse(bad)
            assert False
        except p.ProtocolError as e:
            assert str(e) == err


def test_dedos():
    assert p.parse_fingers("a") == 0b11111
    assert p.parse_fingers("indice,medio,3") == 0b01110


def test_emulador_extremo_a_extremo():
    emu = Emulador("127.0.0.1", 0, 0, 0b01110, verbose=False, announce=False)
    emu.start_thread()
    cli = Cliente("127.0.0.1", emu.port, 0, timeout=1.0)
    try:
        st = cli.status()
        assert st and st.present_mask == 0b01110 and st.fw == "0.4"
        assert cli.ping() is not None
        assert cli.pulse_ack(0b00010, 200, 50) is not None
        cli.send(p.effect(cli.next_seq(), 0b00110, 47))
        r = medir_latencia(cli, 20, 1, False, 0b10, None)
        assert r["perdidos"] == 0 and r["p95_ms"] < 50
        # paquete corrupto => ERROR con el código BAD_CRC
        bad = bytearray(p.stop(99)); bad[-1] ^= 0xFF
        cli.send(bytes(bad))
        try:
            cli.wait(p.STOP, 99)
            assert False
        except p.ProtocolError as e:
            assert "BAD_CRC" in str(e)
        import time; time.sleep(0.05)
        assert any("efecto  47" in m and "indice, medio" in m for m in emu.log)
    finally:
        emu.close()


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn(); print("OK", name)
