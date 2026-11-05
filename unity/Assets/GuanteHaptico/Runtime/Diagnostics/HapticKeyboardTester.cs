using UnityEngine;
#if ENABLE_INPUT_SYSTEM
using UnityEngine.InputSystem;
#endif

namespace GuanteHaptico.Diagnostics
{
    /// <summary>
    /// Pruebas rápidas desde el teclado del PC (modo Editor / PCVR):
    /// 1–5 = pulso en pulgar..meñique, Q = clic en todos, W = secuencia, S = parar, P = ping.
    /// </summary>
    public class HapticKeyboardTester : MonoBehaviour
    {
        [Range(0f, 1f)] public float intensity = 0.8f;
        public int pulseMs = 120;

        void Start()
        {
            if (HapticGlove.Instance != null)
                HapticGlove.Instance.RoundTripMeasured += (t, ms, us) => Debug.Log($"[Guante] {t} RTT {ms:F2} ms (proceso {us} µs)");
        }

        void Update()
        {
#if ENABLE_INPUT_SYSTEM
            var glove = HapticGlove.Instance;
            var kb = Keyboard.current;
            if (glove == null || kb == null) return;
            Key[] fingerKeys = { Key.Digit1, Key.Digit2, Key.Digit3, Key.Digit4, Key.Digit5 };
            for (int i = 0; i < fingerKeys.Length; i++)
                if (kb[fingerKeys[i]].wasPressedThisFrame)
                    glove.Pulse(((Finger)i).ToMask(), intensity, pulseMs);
            if (kb.qKey.wasPressedThisFrame) glove.PlayEffect(FingerMask.Todos, 1);
            if (kb.wKey.wasPressedThisFrame) glove.PlaySequence(FingerMask.Todos, 1, 10, 52);
            if (kb.sKey.wasPressedThisFrame) glove.Stop();
            if (kb.pKey.wasPressedThisFrame) glove.Ping();
#endif
        }
    }
}
