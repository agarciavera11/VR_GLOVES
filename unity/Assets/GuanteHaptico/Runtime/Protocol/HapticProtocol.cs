using System;

namespace GuanteHaptico.Protocol
{
    /// <summary>
    /// Protocolo binario Unity &lt;-&gt; guante, versión 1.
    /// Espejo exacto de firmware/guante_haptico/protocol.cpp y tools/protocolo.py.
    /// Especificación: docs/protocolo.md
    /// </summary>
    public static class HapticProtocol
    {
        public const byte Magic = 0xA7;
        public const byte Version = 1;
        public const int HeaderSize = 6;
        public const int MaxPayload = 24;
        public const int MaxPacket = HeaderSize + MaxPayload + 1;
        public const int MaxSequenceEffects = 8;
        public const byte FlagAckRequest = 0x01;
        public const int StatusPayloadSize = 16;

        public static byte Crc8(byte[] data, int offset, int count)
        {
            byte crc = 0;
            for (int i = offset; i < offset + count; i++)
            {
                crc ^= data[i];
                for (int b = 0; b < 8; b++)
                    crc = (crc & 0x80) != 0 ? (byte)((crc << 1) ^ 0x07) : (byte)(crc << 1);
            }
            return crc;
        }

        static bool LengthIsValid(PacketType t, int len)
        {
            switch (t)
            {
                case PacketType.Ping: return len == 4;
                case PacketType.Effect: return len == 2;
                case PacketType.Pulse: return len == 4;
                case PacketType.Sequence: return len >= 2 && len <= 1 + MaxSequenceEffects;
                case PacketType.Stop: return len == 1;
                case PacketType.StatusRequest: return len == 0;
                case PacketType.Pong: return len == 10;
                case PacketType.Ack: return len == 2;
                case PacketType.Status:
                case PacketType.Announce: return len == StatusPayloadSize;
                case PacketType.Error: return len == 1;
                default: return false;
            }
        }

        /// <summary>Construye un paquete en <paramref name="output"/> y devuelve su tamaño (0 si no cabe).</summary>
        public static int Build(PacketType type, byte flags, ushort seq, byte[] payload, int payloadLength,
            byte[] output)
        {
            int total = HeaderSize + payloadLength + 1;
            if (payloadLength > MaxPayload || output == null || output.Length < total) return 0;
            output[0] = Magic;
            output[1] = Version;
            output[2] = (byte)type;
            output[3] = flags;
            output[4] = (byte)(seq & 0xFF);
            output[5] = (byte)(seq >> 8);
            if (payloadLength > 0) Buffer.BlockCopy(payload, 0, output, HeaderSize, payloadLength);
            output[total - 1] = Crc8(output, 0, total - 1);
            return total;
        }

        public static byte[] Build(PacketType type, byte flags, ushort seq, params byte[] payload)
        {
            payload = payload ?? Array.Empty<byte>();
            var buf = new byte[HeaderSize + payload.Length + 1];
            if (Build(type, flags, seq, payload, payload.Length, buf) == 0)
                throw new ArgumentException("payload demasiado grande");
            return buf;
        }

        public static ParseResult TryParse(byte[] buf, int length, out Packet packet)
        {
            packet = default;
            if (buf == null || length < HeaderSize + 1) return ParseResult.TooShort;
            if (buf[0] != Magic) return ParseResult.BadMagic;
            if (buf[1] != Version) return ParseResult.BadVersion;
            if (length > MaxPacket) return ParseResult.BadLength;
            if (Crc8(buf, 0, length - 1) != buf[length - 1]) return ParseResult.BadCrc;
            var type = (PacketType)buf[2];
            if (!Enum.IsDefined(typeof(PacketType), type)) return ParseResult.UnknownType;
            int len = length - HeaderSize - 1;
            if (!LengthIsValid(type, len)) return ParseResult.BadLength;
            var payload = new byte[len];
            Buffer.BlockCopy(buf, HeaderSize, payload, 0, len);
            packet = new Packet(type, buf[3], (ushort)(buf[4] | (buf[5] << 8)), payload);
            return ParseResult.Ok;
        }

        // ---- Ayudas little-endian ----
        public static void PutU16(byte[] b, int o, ushort v) { b[o] = (byte)v; b[o + 1] = (byte)(v >> 8); }
        public static void PutU32(byte[] b, int o, uint v)
        {
            for (int i = 0; i < 4; i++) b[o + i] = (byte)(v >> (8 * i));
        }
        public static ushort GetU16(byte[] b, int o) => (ushort)(b[o] | (b[o + 1] << 8));
        public static uint GetU32(byte[] b, int o) =>
            (uint)(b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24));

        // ---- Payloads de comandos ----
        public static byte[] PingPayload(uint echo) { var p = new byte[4]; PutU32(p, 0, echo); return p; }
        public static byte[] EffectPayload(FingerMask mask, byte effectId) => new[] { (byte)mask, effectId };

        public static byte[] PulsePayload(FingerMask mask, byte amplitude, ushort durationMs)
        {
            var p = new byte[4];
            p[0] = (byte)mask;
            p[1] = amplitude;
            PutU16(p, 2, durationMs);
            return p;
        }

        public static byte[] SequencePayload(FingerMask mask, byte[] effectIds)
        {
            if (effectIds == null || effectIds.Length < 1 || effectIds.Length > MaxSequenceEffects)
                throw new ArgumentException("la secuencia debe tener entre 1 y 8 efectos");
            var p = new byte[1 + effectIds.Length];
            p[0] = (byte)mask;
            Buffer.BlockCopy(effectIds, 0, p, 1, effectIds.Length);
            return p;
        }

        public static bool TryDecodeStatus(byte[] p, out DeviceStatus s)
        {
            s = default;
            if (p == null || p.Length != StatusPayloadSize) return false;
            s = new DeviceStatus
            {
                FirmwareMajor = p[0],
                FirmwareMinor = p[1],
                FingerCount = p[2],
                PresentMask = (FingerMask)p[3],
                CalibratedMask = (FingerMask)p[4],
                Rssi = (sbyte)p[5],
                UptimeMs = GetU32(p, 6),
                RxPackets = GetU32(p, 10),
                RxErrors = GetU16(p, 14),
            };
            return true;
        }
    }

    public enum PacketType : byte
    {
        Ping = 0x01,
        Effect = 0x10,
        Pulse = 0x11,
        Sequence = 0x12,
        Stop = 0x13,
        StatusRequest = 0x20,
        Pong = 0x81,
        Ack = 0x82,
        Status = 0xA0,
        Announce = 0xA1,
        Error = 0xEE,
    }

    public enum ParseResult : byte
    {
        Ok = 0,
        TooShort = 1,
        BadMagic = 2,
        BadVersion = 3,
        BadCrc = 4,
        BadLength = 5,
        UnknownType = 6,
    }

    public readonly struct Packet
    {
        public readonly PacketType Type;
        public readonly byte Flags;
        public readonly ushort Seq;
        public readonly byte[] Payload;

        public Packet(PacketType type, byte flags, ushort seq, byte[] payload)
        {
            Type = type;
            Flags = flags;
            Seq = seq;
            Payload = payload;
        }
    }

    [Serializable]
    public struct DeviceStatus
    {
        public byte FirmwareMajor;
        public byte FirmwareMinor;
        public byte FingerCount;
        public FingerMask PresentMask;
        public FingerMask CalibratedMask;
        public sbyte Rssi;
        public uint UptimeMs;
        public uint RxPackets;
        public ushort RxErrors;

        public override string ToString() =>
            $"fw {FirmwareMajor}.{FirmwareMinor} | dedos {PresentMask} | calibrados {CalibratedMask} | " +
            $"RSSI {Rssi} dBm | rx {RxPackets} | errores {RxErrors}";
    }
}
