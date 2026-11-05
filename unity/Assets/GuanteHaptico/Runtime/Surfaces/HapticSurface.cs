using System.Collections.Generic;
using GuanteHaptico.Hands;
using UnityEngine;

namespace GuanteHaptico.Surfaces
{
    /// <summary>
    /// Objeto que se "siente" al tocarlo. Ponlo en cualquier objeto con Collider.
    /// - Al entrar: impacto (efecto de librería o pulso con intensidad según la velocidad).
    /// - Mientras se toca (opcional): vibración sostenida tipo textura.
    /// </summary>
    public class HapticSurface : MonoBehaviour
    {
        public enum ImpactMode
        {
            [Tooltip("Efecto de la librería LRA del DRV2605L")] LibraryEffect,
            [Tooltip("Pulso RTP con intensidad proporcional a la velocidad")] VelocityPulse,
            None,
        }

        [Header("Impacto al tocar")]
        public ImpactMode impact = ImpactMode.VelocityPulse;
        [Range(1, 123)] public int effectId = 1;
        [Range(0f, 1f)] public float minIntensity = 0.35f;
        [Range(0f, 1f)] public float maxIntensity = 1f;
        [Tooltip("Velocidad de la yema (m/s) a la que se alcanza la intensidad máxima.")]
        public float speedForMax = 0.4f;
        [Range(5, 500)] public int impactMs = 40;
        [Tooltip("Tiempo mínimo entre impactos del mismo dedo (evita rebotes).")]
        public float cooldownSeconds = 0.08f;

        [Header("Contacto sostenido")]
        public bool sustainWhileTouching;
        [Range(0f, 1f)] public float sustainIntensity = 0.25f;

        // El pulso sostenido se renueva antes de que caduque en el firmware.
        const int k_SustainPulseMs = 150;
        const float k_SustainRefresh = 0.1f;

        readonly Dictionary<Finger, float> m_LastImpact = new Dictionary<Finger, float>();
        protected FingerMask touchingMask;
        float m_NextSustain;

        public FingerMask TouchingMask => touchingMask;

        protected static HapticGlove Glove => HapticGlove.Instance;

        public virtual void TouchBegin(FingertipSensor sensor, float speed)
        {
            touchingMask |= sensor.finger.ToMask();
            if (Glove == null) return;
            float now = Time.time;
            if (m_LastImpact.TryGetValue(sensor.finger, out float last) && now - last < cooldownSeconds) return;
            m_LastImpact[sensor.finger] = now;

            switch (impact)
            {
                case ImpactMode.LibraryEffect:
                    Glove.PlayEffect(sensor.finger.ToMask(), (byte)effectId);
                    break;
                case ImpactMode.VelocityPulse:
                    float t = speedForMax > 0 ? Mathf.Clamp01(speed / speedForMax) : 1f;
                    Glove.Pulse(sensor.finger.ToMask(), Mathf.Lerp(minIntensity, maxIntensity, t), impactMs);
                    break;
            }
        }

        public virtual void TouchEnd(FingertipSensor sensor)
        {
            var m = sensor.finger.ToMask();
            touchingMask &= ~m;
            if (sustainWhileTouching && Glove != null) Glove.Stop(m);
        }

        protected virtual void Update()
        {
            if (!sustainWhileTouching || touchingMask == FingerMask.Ninguno || Glove == null) return;
            if (Time.time < m_NextSustain) return;
            m_NextSustain = Time.time + k_SustainRefresh;
            Glove.Pulse(touchingMask, sustainIntensity, k_SustainPulseMs);
        }

        protected virtual void OnDisable()
        {
            if (touchingMask != FingerMask.Ninguno && Glove != null) Glove.Stop(touchingMask);
            touchingMask = FingerMask.Ninguno;
        }
    }
}
