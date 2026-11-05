using GuanteHaptico.Protocol;
using NUnit.Framework;

namespace GuanteHaptico.Tests
{
    /// <summary>
    /// Mismos vectores que docs/golden_vectors.txt (generados por el firmware compilado en PC):
    /// garantizan que Unity, el firmware y las herramientas Python hablan exactamente igual.
    /// </summary>
    public class HapticProtocolTests
    {
        static byte[] Hex(string h)
        {
            var b = new byte[h.Length / 2];
            for (int i = 0; i < b.Length; i++) b[i] = System.Convert.ToByte(h.Substring(2 * i, 2), 16);
            return b;
        }

        [Test]
        public void Crc8_ValorDeReferencia()
        {
            var data = System.Text.Encoding.ASCII.GetBytes("123456789");
            Assert.AreEqual(0xF4, HapticProtocol.Crc8(data, 0, data.Length));
        }

        [TestCase(PacketType.Ping, (ushort)1, (byte)0, "78563412", "A7010100010078563412FB")]
        [TestCase(PacketType.Effect, (ushort)2, (byte)0, "022F", "A70110000200022F22")]
        [TestCase(PacketType.Pulse, (ushort)258, (byte)1, "1FFF2C01", "A701110102011FFF2C01C9")]
        [TestCase(PacketType.Sequence, (ushort)3, (byte)0, "0E010A34", "A701120003000E010A3402")]
        [TestCase(PacketType.Stop, (ushort)65535, (byte)0, "FF", "A7011300FFFFFF8A")]
        [TestCase(PacketType.StatusRequest, (ushort)7, (byte)0, "", "A70120000700CF")]
        public void Build_y_Parse_CoincidenConElFirmware(PacketType type, ushort seq, byte flags, string payload,
            string expected)
        {
            var bytes = HapticProtocol.Build(type, flags, seq, Hex(payload));
            Assert.AreEqual(expected, System.BitConverter.ToString(bytes).Replace("-", ""));
            Assert.AreEqual(ParseResult.Ok, HapticProtocol.TryParse(bytes, bytes.Length, out var pkt));
            Assert.AreEqual(type, pkt.Type);
            Assert.AreEqual(seq, pkt.Seq);
            CollectionAssert.AreEqual(Hex(payload), pkt.Payload);
        }

        [Test]
        public void Parse_DetectaErrores()
        {
            var good = HapticProtocol.Build(PacketType.Pulse, 0, 1,
                HapticProtocol.PulsePayload(FingerMask.Indice, 200, 80));
            var bad = (byte[])good.Clone();
            bad[bad.Length - 1] ^= 0xFF;
            Assert.AreEqual(ParseResult.BadCrc, HapticProtocol.TryParse(bad, bad.Length, out _));
            bad = (byte[])good.Clone();
            bad[0] = 0;
            Assert.AreEqual(ParseResult.BadMagic, HapticProtocol.TryParse(bad, bad.Length, out _));
            Assert.AreEqual(ParseResult.TooShort, HapticProtocol.TryParse(good, 3, out _));
            var shortEffect = HapticProtocol.Build(PacketType.Effect, 0, 1, 0x01);
            Assert.AreEqual(ParseResult.BadLength, HapticProtocol.TryParse(shortEffect, shortEffect.Length, out _));
        }

        [Test]
        public void Status_SeDecodifica()
        {
            var p = new byte[] { 0, 4, 5, 0x0E, 0x0C, unchecked((byte)-60), 0x40, 0xE2, 0x01, 0, 42, 0, 0, 0, 3, 0 };
            Assert.IsTrue(HapticProtocol.TryDecodeStatus(p, out var s));
            Assert.AreEqual(FingerMask.Indice | FingerMask.Medio | FingerMask.Anular, s.PresentMask);
            Assert.AreEqual(-60, s.Rssi);
            Assert.AreEqual(123456u, s.UptimeMs);
            Assert.AreEqual(42u, s.RxPackets);
        }
    }
}
