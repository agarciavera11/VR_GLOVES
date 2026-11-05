using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using GuanteHaptico.Protocol;

namespace GuanteHaptico.Transport
{
    /// <summary>Paquete recibido con la marca de tiempo (Stopwatch) del instante de llegada.</summary>
    public readonly struct ReceivedPacket
    {
        public readonly Packet Packet;
        public readonly IPEndPoint From;
        public readonly long TimestampTicks;

        public ReceivedPacket(Packet packet, IPEndPoint from, long ts)
        {
            Packet = packet;
            From = from;
            TimestampTicks = ts;
        }
    }

    /// <summary>
    /// Transporte UDP. Un único socket ligado al puerto local (8889) envía comandos al guante y
    /// recibe respuestas y anuncios broadcast. La recepción corre en un hilo propio y deja los
    /// paquetes en una cola que se vacía desde el hilo principal de Unity.
    /// </summary>
    public sealed class UdpHapticTransport : IDisposable
    {
        readonly UdpClient m_Client;
        readonly Thread m_Thread;
        readonly ConcurrentQueue<ReceivedPacket> m_Inbox = new ConcurrentQueue<ReceivedPacket>();
        readonly byte[] m_SendBuffer = new byte[HapticProtocol.MaxPacket];
        readonly object m_SendLock = new object();
        volatile bool m_Running;

        public IPEndPoint Remote { get; set; }
        public int ParseErrors { get; private set; }
        public int LocalPort { get; }

        public UdpHapticTransport(int localPort)
        {
            m_Client = new UdpClient();
            m_Client.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
            m_Client.EnableBroadcast = true;
            m_Client.Client.Bind(new IPEndPoint(IPAddress.Any, localPort));
            LocalPort = ((IPEndPoint)m_Client.Client.LocalEndPoint).Port;
            m_Running = true;
            m_Thread = new Thread(ReceiveLoop) { IsBackground = true, Name = "GuanteHaptico.UDP" };
            m_Thread.Start();
        }

        void ReceiveLoop()
        {
            var any = new IPEndPoint(IPAddress.Any, 0);
            while (m_Running)
            {
                try
                {
                    byte[] data = m_Client.Receive(ref any);
                    long ts = Stopwatch.GetTimestamp();
                    if (HapticProtocol.TryParse(data, data.Length, out var pkt) == ParseResult.Ok)
                        m_Inbox.Enqueue(new ReceivedPacket(pkt, new IPEndPoint(any.Address, any.Port), ts));
                    else
                        ParseErrors++;
                }
                catch (SocketException)
                {
                    // Socket cerrado al salir, o ICMP "port unreachable" si el guante no escucha.
                    if (!m_Running) return;
                }
                catch (ObjectDisposedException)
                {
                    return;
                }
            }
        }

        public bool TryDequeue(out ReceivedPacket p) => m_Inbox.TryDequeue(out p);

        /// <summary>Envía un paquete al guante. Devuelve la marca de tiempo del envío (o -1).</summary>
        public long Send(PacketType type, byte flags, ushort seq, byte[] payload)
        {
            if (Remote == null) return -1;
            lock (m_SendLock)
            {
                int n = HapticProtocol.Build(type, flags, seq, payload, payload?.Length ?? 0, m_SendBuffer);
                if (n == 0) return -1;
                long ts = Stopwatch.GetTimestamp();
                try
                {
                    m_Client.Send(m_SendBuffer, n, Remote);
                }
                catch (SocketException)
                {
                    return -1;
                }
                return ts;
            }
        }

        public void Dispose()
        {
            m_Running = false;
            m_Client.Close();
            m_Thread.Join(200);
        }
    }
}
