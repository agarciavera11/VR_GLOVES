using System.Collections.Generic;
using GuanteHaptico.Surfaces;
using UnityEngine;

namespace GuanteHaptico.Hands
{
    /// <summary>
    /// Esfera de contacto de una yema. Detecta entrada/salida en <see cref="HapticSurface"/> y
    /// estima la velocidad de la yema para escalar la intensidad del impacto.
    /// </summary>
    [RequireComponent(typeof(Collider))]
    public class FingertipSensor : MonoBehaviour
    {
        public Finger finger;

        /// <summary>Velocidad de la yema en m/s (espacio mundo), suavizada.</summary>
        public Vector3 Velocity { get; private set; }

        // Contador por superficie: una superficie puede tener varios colliders hijos.
        readonly Dictionary<HapticSurface, int> m_Touching = new Dictionary<HapticSurface, int>();
        readonly List<HapticSurface> m_Scratch = new List<HapticSurface>();
        Vector3 m_LastPos;
        bool m_HasLast;

        void OnEnable() => m_HasLast = false;

        void OnDisable()
        {
            // Si se pierde el seguimiento hay que cerrar los contactos abiertos.
            m_Scratch.Clear();
            m_Scratch.AddRange(m_Touching.Keys);
            m_Touching.Clear();
            foreach (var s in m_Scratch)
                if (s != null) s.TouchEnd(this);
        }

        void FixedUpdate()
        {
            Vector3 p = transform.position;
            if (m_HasLast)
            {
                Vector3 v = (p - m_LastPos) / Time.fixedDeltaTime;
                Velocity = Vector3.Lerp(Velocity, v, 0.5f);
            }
            m_LastPos = p;
            m_HasLast = true;
        }

        void OnTriggerEnter(Collider other)
        {
            var surface = other.GetComponentInParent<HapticSurface>();
            if (surface == null || !surface.isActiveAndEnabled) return;
            m_Touching.TryGetValue(surface, out int n);
            m_Touching[surface] = n + 1;
            if (n == 0) surface.TouchBegin(this, Velocity.magnitude);
        }

        void OnTriggerExit(Collider other)
        {
            var surface = other.GetComponentInParent<HapticSurface>();
            if (surface == null || !m_Touching.TryGetValue(surface, out int n)) return;
            if (n > 1)
            {
                m_Touching[surface] = n - 1;
                return;
            }
            m_Touching.Remove(surface);
            surface.TouchEnd(this);
        }
    }
}
