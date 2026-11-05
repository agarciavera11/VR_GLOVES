using UnityEngine;

namespace GuanteHaptico.Diagnostics
{
    /// <summary>
    /// Texto en el mundo (visible con el visor puesto) con el estado del guante y la latencia.
    /// Añádelo a un objeto con TextMesh, o se crea uno.
    /// </summary>
    public class HapticStatusDisplay : MonoBehaviour
    {
        public LatencyProbe probe;
        public float refreshSeconds = 0.25f;

        TextMesh m_Text;
        float m_Next;

        void Awake()
        {
            m_Text = GetComponent<TextMesh>();
            if (m_Text == null)
            {
                m_Text = gameObject.AddComponent<TextMesh>();
                m_Text.characterSize = 0.01f;
                m_Text.fontSize = 48;
                m_Text.anchor = TextAnchor.UpperLeft;
            }
        }

        void Update()
        {
            if (Time.unscaledTime < m_Next) return;
            m_Next = Time.unscaledTime + refreshSeconds;
            var g = HapticGlove.Instance;
            if (g == null)
            {
                m_Text.text = "Sin HapticGlove en la escena";
                return;
            }
            string s = g.IsConnected
                ? $"Guante: {g.DeviceEndPoint}\n{g.LastStatus}"
                : g.DeviceEndPoint == null ? "Guante: buscando..." : $"Guante: sin respuesta ({g.DeviceEndPoint})";
            if (probe != null && probe.Received > 0)
                s += $"\nRTT p50 {probe.P50Ms:F1} ms | p95 {probe.P95Ms:F1} ms | máx {probe.MaxMs:F1} ms";
            m_Text.text = s;
        }
    }
}
