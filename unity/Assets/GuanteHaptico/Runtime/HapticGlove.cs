using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
using GuanteHaptico.Protocol;
using GuanteHaptico.Transport;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace GuanteHaptico
{
    /// <summary>
    /// Punto de entrada para mandar retroalimentación háptica al guante.
    /// Pon un único HapticGlove en la escena (por ejemplo en el XR Origin).
    /// Si <see cref="deviceIp"/> está vacío, la dirección se descubre sola con el ANNOUNCE del guante.
    /// </summary>
    [DefaultExecutionOrder(-100)]
    public class HapticGlove : MonoBehaviour
    {
        public static HapticGlove Instance { get; private set; }

        [Header("Red")]
        [Tooltip("IP del guante. Vacío = descubrimiento automático por broadcast.")]
        public string deviceIp = "";
        public int devicePort = 8888;
        [Tooltip("Puerto local donde llegan respuestas y anuncios.")]
        public int localPort = 8889;

        [Header("Mantenimiento")]
        [Tooltip("Cada cuánto se pide STATUS (mantiene viva la sesión en el firmware).")]
        public float statusIntervalSeconds = 1f;
        [Tooltip("Sin respuestas durante este tiempo => desconectado.")]
        public float connectionTimeoutSeconds = 3f;

        [Header("Depuración")]
        public bool logCommands;

        public bool IsConnected => m_LastRxTime > 0 && Time.unscaledTime - m_LastRxTime < connectionTimeoutSeconds;
        public DeviceStatus LastStatus { get; private set; }
        public IPEndPoint DeviceEndPoint => m_Transport?.Remote;
        public int ParseErrors => m_Transport?.ParseErrors ?? 0;

        /// <summary>RTT en ms y µs de proceso informados por el guante, por cada PONG o ACK.</summary>
        public event Action<PacketType, double, int> RoundTripMeasured;
        public event Action<DeviceStatus> StatusReceived;
        public event Action<IPEndPoint> DeviceDiscovered;

        UdpHapticTransport m_Transport;
        ushort m_Seq;
        float m_LastRxTime = -1;
        float m_NextStatusTime;
        readonly Dictionary<ushort, long> m_PendingSends = new Dictionary<ushort, long>();
        static readonly double s_TicksToMs = 1000.0 / Stopwatch.Frequency;

        void Awake()
        {
            if (Instance != null && Instance != this)
            {
                Debug.LogWarning("[Guante] Ya existe un HapticGlove en la escena; se desactiva este.", this);
                enabled = false;
                return;
            }
            Instance = this;
        }

        void OnEnable()
        {
            try
            {
                m_Transport = new UdpHapticTransport(localPort);
            }
            catch (Exception e)
            {
                Debug.LogError($"[Guante] No se pudo abrir el puerto UDP {localPort}: {e.Message}", this);
                return;
            }
            if (!string.IsNullOrWhiteSpace(deviceIp) && IPAddress.TryParse(deviceIp.Trim(), out var ip))
                m_Transport.Remote = new IPEndPoint(ip, devicePort);
            else
                Debug.Log($"[Guante] Esperando anuncio del guante en el puerto {localPort}...");
        }

        void OnDisable()
        {
            if (m_Transport != null)
            {
                Stop(FingerMask.Todos);
                m_Transport.Dispose();
                m_Transport = null;
            }
            m_PendingSends.Clear();
        }

        void OnDestroy()
        {
            if (Instance == this) Instance = null;
        }

        void Update()
        {
            if (m_Transport == null) return;
            while (m_Transport.TryDequeue(out var rp)) Handle(rp);

            if (m_Transport.Remote != null && Time.unscaledTime >= m_NextStatusTime)
            {
                m_NextStatusTime = Time.unscaledTime + statusIntervalSeconds;
                Send(PacketType.StatusRequest, 0, null);
            }
            // Limpia envíos sin respuesta (paquetes perdidos)
            if (m_PendingSends.Count > 256) m_PendingSends.Clear();
        }

        void Handle(in ReceivedPacket rp)
        {
            var pkt = rp.Packet;
            m_LastRxTime = Time.unscaledTime;
            switch (pkt.Type)
            {
                case PacketType.Announce:
                    if (m_Transport.Remote == null || !m_Transport.Remote.Address.Equals(rp.From.Address))
                    {
                        if (m_Transport.Remote != null && !string.IsNullOrWhiteSpace(deviceIp)) break; // IP fija
                        m_Transport.Remote = new IPEndPoint(rp.From.Address, devicePort);
                        Debug.Log($"[Guante] Encontrado en {rp.From.Address}");
                        DeviceDiscovered?.Invoke(m_Transport.Remote);
                        m_NextStatusTime = 0;
                    }
                    goto case PacketType.Status;
                case PacketType.Status:
                    if (HapticProtocol.TryDecodeStatus(pkt.Payload, out var st))
                    {
                        LastStatus = st;
                        StatusReceived?.Invoke(st);
                    }
                    break;
                case PacketType.Pong:
                    ReportRtt(pkt.Type, pkt.Seq, rp.TimestampTicks, HapticProtocol.GetU16(pkt.Payload, 8));
                    break;
                case PacketType.Ack:
                    ReportRtt(pkt.Type, pkt.Seq, rp.TimestampTicks, HapticProtocol.GetU16(pkt.Payload, 0));
                    break;
                case PacketType.Error:
                    Debug.LogWarning($"[Guante] El firmware rechazó el paquete {pkt.Seq}: {(ParseResult)pkt.Payload[0]}");
                    break;
            }
        }

        void ReportRtt(PacketType type, ushort seq, long rxTicks, int procUs)
        {
            if (!m_PendingSends.TryGetValue(seq, out long txTicks)) return;
            m_PendingSends.Remove(seq);
            RoundTripMeasured?.Invoke(type, (rxTicks - txTicks) * s_TicksToMs, procUs);
        }

        ushort Send(PacketType type, byte flags, byte[] payload, bool track = false)
        {
            if (m_Transport == null || m_Transport.Remote == null) return 0;
            ushort seq = ++m_Seq;
            long ts = m_Transport.Send(type, flags, seq, payload);
            if (track && ts >= 0) m_PendingSends[seq] = ts;
            if (logCommands && type != PacketType.StatusRequest && type != PacketType.Ping)
                Debug.Log($"[Guante] -> {type} seq={seq}");
            return seq;
        }

        // ------------------------------------------------------------------ API pública

        /// <summary>Reproduce un efecto de la librería LRA del DRV2605L (1..123).</summary>
        public void PlayEffect(FingerMask fingers, byte effectId, bool requestAck = false) =>
            Send(PacketType.Effect, requestAck ? HapticProtocol.FlagAckRequest : (byte)0,
                HapticProtocol.EffectPayload(fingers, effectId), requestAck);

        /// <summary>Vibración continua en tiempo real. intensity 0..1, duración en ms (máx. 2000).</summary>
        public void Pulse(FingerMask fingers, float intensity, int durationMs, bool requestAck = false)
        {
            byte amp = (byte)Mathf.RoundToInt(Mathf.Clamp01(intensity) * 255f);
            ushort dur = (ushort)Mathf.Clamp(durationMs, 0, 2000);
            Send(PacketType.Pulse, requestAck ? HapticProtocol.FlagAckRequest : (byte)0,
                HapticProtocol.PulsePayload(fingers, amp, dur), requestAck);
        }

        /// <summary>Hasta 8 efectos de librería encadenados.</summary>
        public void PlaySequence(FingerMask fingers, params byte[] effectIds) =>
            Send(PacketType.Sequence, 0, HapticProtocol.SequencePayload(fingers, effectIds));

        public void Stop(FingerMask fingers = FingerMask.Todos) =>
            Send(PacketType.Stop, 0, new[] { (byte)fingers });

        /// <summary>Envía un PING; el RTT llega por <see cref="RoundTripMeasured"/>.</summary>
        public ushort Ping()
        {
            uint echo = (uint)(Stopwatch.GetTimestamp() & 0xFFFFFFFF);
            return Send(PacketType.Ping, 0, HapticProtocol.PingPayload(echo), true);
        }
    }
}
