using UnityEngine;

namespace GuanteHaptico.Surfaces
{
    /// <summary>
    /// Zona de pulso para palpación (p. ej. arteria radial). Mientras un dedo la toca, recibe
    /// latidos al ritmo indicado. Reproduce los casos de la tesis base (45/60/120 ppm) y los
    /// patrones normal / hipotenso / hipertenso.
    /// </summary>
    public class PulseZone : HapticSurface
    {
        public enum Pattern { Normal, Hipotenso, Hipertenso }

        [Header("Pulso")]
        [Range(30, 180)] public int beatsPerMinute = 60;
        public Pattern pattern = Pattern.Normal;
        [Tooltip("Añade la onda dícrota (segundo golpe más débil) tras cada latido.")]
        public bool dicroticNotch = true;

        float m_NextBeat;
        float m_PendingSecond = -1;

        void Reset()
        {
            impact = ImpactMode.None;
            sustainWhileTouching = false;
        }

        (float amp, int ms) Beat()
        {
            switch (pattern)
            {
                case Pattern.Hipotenso: return (0.30f, 45);
                case Pattern.Hipertenso: return (1.00f, 90);
                default: return (0.65f, 60);
            }
        }

        public override void TouchBegin(Hands.FingertipSensor sensor, float speed)
        {
            bool wasEmpty = touchingMask == FingerMask.Ninguno;
            base.TouchBegin(sensor, speed);
            if (wasEmpty) m_NextBeat = Time.time + 0.05f; // primer latido casi inmediato
        }

        protected override void Update()
        {
            base.Update();
            if (touchingMask == FingerMask.Ninguno || Glove == null)
            {
                m_PendingSecond = -1;
                return;
            }
            float now = Time.time;
            var (amp, ms) = Beat();
            if (now >= m_NextBeat)
            {
                Glove.Pulse(touchingMask, amp, ms);
                m_NextBeat += 60f / beatsPerMinute;
                if (m_NextBeat < now) m_NextBeat = now + 60f / beatsPerMinute;
                m_PendingSecond = dicroticNotch && pattern != Pattern.Hipotenso ? now + 0.18f : -1;
            }
            if (m_PendingSecond > 0 && now >= m_PendingSecond)
            {
                Glove.Pulse(touchingMask, amp * 0.4f, ms / 2);
                m_PendingSecond = -1;
            }
        }
    }
}
