using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using GuanteHaptico.Protocol;
using UnityEngine;

namespace GuanteHaptico.Diagnostics
{
    /// <summary>
    /// Mide el tiempo de ida y vuelta (RTT) Unity -> guante -> Unity con PING (solo red) o con
    /// PULSE+ACK (red + ejecución I2C en el firmware). Guarda las muestras en CSV para el informe.
    /// La latencia de ida se estima como RTT/2.
    /// </summary>
    public class LatencyProbe : MonoBehaviour
    {
        public enum ProbeKind { Ping, PulseWithAck }

        public ProbeKind kind = ProbeKind.Ping;
        [Range(1, 100)] public float probesPerSecond = 20f;
        [Tooltip("Muestras para las estadísticas (ventana móvil).")]
        public int window = 500;
        public bool runOnStart;
        [Tooltip("Dedo para PulseWithAck (vibra muy suave y breve).")]
        public Finger probeFinger = Finger.Indice;

        public bool Running { get; private set; }
        public int Sent { get; private set; }
        public int Received { get; private set; }
        public float MeanMs { get; private set; }
        public float P50Ms { get; private set; }
        public float P95Ms { get; private set; }
        public float MaxMs { get; private set; }

        readonly List<float> m_Window = new List<float>();
        readonly List<float> m_Sorted = new List<float>();
        readonly StringBuilder m_Csv = new StringBuilder();
        float m_Next;

        void OnEnable()
        {
            if (HapticGlove.Instance != null) HapticGlove.Instance.RoundTripMeasured += OnRtt;
            if (runOnStart) StartProbe();
        }

        void OnDisable()
        {
            if (HapticGlove.Instance != null) HapticGlove.Instance.RoundTripMeasured -= OnRtt;
            Running = false;
        }

        [ContextMenu("Iniciar medición")]
        public void StartProbe()
        {
            m_Window.Clear();
            m_Csv.Clear();
            m_Csv.AppendLine("t_s,tipo,rtt_ms,proceso_us");
            Sent = Received = 0;
            Running = true;
        }

        [ContextMenu("Detener y guardar CSV")]
        public string StopAndSave()
        {
            Running = false;
            string path = Path.Combine(Application.persistentDataPath,
                $"latencia_{System.DateTime.Now:yyyyMMdd_HHmmss}.csv");
            File.WriteAllText(path, m_Csv.ToString());
            Debug.Log($"[Latencia] {Received}/{Sent} respuestas | media {MeanMs:F2} ms | p50 {P50Ms:F2} | " +
                      $"p95 {P95Ms:F2} | máx {MaxMs:F2}. CSV: {path}");
            return path;
        }

        void Update()
        {
            var glove = HapticGlove.Instance;
            if (!Running || glove == null || !glove.IsConnected) return;
            if (Time.unscaledTime < m_Next) return;
            m_Next = Time.unscaledTime + 1f / probesPerSecond;
            Sent++;
            if (kind == ProbeKind.Ping) glove.Ping();
            else glove.Pulse(probeFinger.ToMask(), 0.1f, 10, requestAck: true);
        }

        void OnRtt(PacketType type, double rttMs, int procUs)
        {
            if (!Running) return;
            Received++;
            m_Csv.Append(Time.unscaledTime.ToString("F3", CultureInfo.InvariantCulture)).Append(',')
                .Append(type).Append(',')
                .Append(rttMs.ToString("F3", CultureInfo.InvariantCulture)).Append(',')
                .Append(procUs).AppendLine();
            m_Window.Add((float)rttMs);
            if (m_Window.Count > window) m_Window.RemoveAt(0);
            Recompute();
        }

        void Recompute()
        {
            m_Sorted.Clear();
            m_Sorted.AddRange(m_Window);
            m_Sorted.Sort();
            float sum = 0;
            foreach (var v in m_Sorted) sum += v;
            MeanMs = sum / m_Sorted.Count;
            P50Ms = Percentile(0.5f);
            P95Ms = Percentile(0.95f);
            MaxMs = m_Sorted[m_Sorted.Count - 1];
        }

        float Percentile(float q)
        {
            float k = (m_Sorted.Count - 1) * q;
            int lo = Mathf.FloorToInt(k), hi = Mathf.Min(lo + 1, m_Sorted.Count - 1);
            return Mathf.Lerp(m_Sorted[lo], m_Sorted[hi], k - lo);
        }
    }
}
