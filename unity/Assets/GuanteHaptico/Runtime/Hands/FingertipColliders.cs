using System.Collections.Generic;
using Unity.XR.CoreUtils;
using UnityEngine;
using UnityEngine.XR.Hands;

namespace GuanteHaptico.Hands
{
    /// <summary>
    /// Crea una esfera de contacto (trigger + Rigidbody cinemático) en la yema de cada dedo y la
    /// mueve con las articulaciones de XR Hands (seguimiento óptico del Quest 3S).
    /// Cada esfera lleva un <see cref="FingertipSensor"/> que avisa a las superficies hápticas.
    /// </summary>
    [DefaultExecutionOrder(-50)]
    public class FingertipColliders : MonoBehaviour
    {
        [Tooltip("Mano que lleva el guante.")]
        public Handedness handedness = Handedness.Right;

        [Tooltip("Espacio de seguimiento: el 'Camera Offset' del XR Origin. Vacío = se busca solo.")]
        public Transform trackingSpace;

        [Tooltip("Radio de la esfera de contacto en metros (yema ≈ 8–10 mm).")]
        public float radius = 0.009f;

        [Tooltip("Dedos que tienen actuador. Los demás no generan contactos.")]
        public FingerMask enabledFingers = FingerMask.Todos;

        [Tooltip("Capa de las esferas (útil para filtrar colisiones en Physics settings).")]
        public int layer = 0;

        public bool showGizmos = true;

        static readonly XRHandJointID[] k_TipJoints =
        {
            XRHandJointID.ThumbTip, XRHandJointID.IndexTip, XRHandJointID.MiddleTip,
            XRHandJointID.RingTip, XRHandJointID.LittleTip,
        };

        static readonly List<XRHandSubsystem> s_Subsystems = new List<XRHandSubsystem>();

        readonly FingertipSensor[] m_Sensors = new FingertipSensor[FingerExtensions.Count];
        XRHandSubsystem m_Subsystem;

        public bool IsTracked { get; private set; }
        public FingertipSensor GetSensor(Finger f) => m_Sensors[(int)f];

        void Awake()
        {
            if (trackingSpace == null)
            {
                var origin = FindAnyObjectByType<XROrigin>();
                trackingSpace = origin != null && origin.CameraFloorOffsetObject != null
                    ? origin.CameraFloorOffsetObject.transform
                    : transform;
            }

            for (int i = 0; i < FingerExtensions.Count; i++)
            {
                var finger = (Finger)i;
                var go = new GameObject($"Yema_{finger}") { layer = layer };
                go.transform.SetParent(trackingSpace, false);
                var col = go.AddComponent<SphereCollider>();
                col.isTrigger = true;
                col.radius = radius;
                var rb = go.AddComponent<Rigidbody>();
                rb.isKinematic = true;
                rb.useGravity = false;
                rb.interpolation = RigidbodyInterpolation.None;
                var sensor = go.AddComponent<FingertipSensor>();
                sensor.finger = finger;
                m_Sensors[i] = sensor;
                go.SetActive(false);
            }
        }

        void OnDisable()
        {
            Unsubscribe();
            SetAllActive(false);
        }

        void OnDestroy()
        {
            foreach (var s in m_Sensors)
                if (s != null) Destroy(s.gameObject);
        }

        void Update()
        {
            if (m_Subsystem == null || !m_Subsystem.running) FindSubsystem();
        }

        void FindSubsystem()
        {
            SubsystemManager.GetSubsystems(s_Subsystems);
            foreach (var s in s_Subsystems)
            {
                if (!s.running) continue;
                Unsubscribe();
                m_Subsystem = s;
                m_Subsystem.updatedHands += OnUpdatedHands;
                return;
            }
        }

        void Unsubscribe()
        {
            if (m_Subsystem != null) m_Subsystem.updatedHands -= OnUpdatedHands;
            m_Subsystem = null;
        }

        void OnUpdatedHands(XRHandSubsystem subsystem, XRHandSubsystem.UpdateSuccessFlags flags,
            XRHandSubsystem.UpdateType updateType)
        {
            // Dynamic = antes de la física de este frame; BeforeRender solo afecta a lo visual.
            if (updateType != XRHandSubsystem.UpdateType.Dynamic) return;

            var hand = handedness == Handedness.Left ? subsystem.leftHand : subsystem.rightHand;
            IsTracked = hand.isTracked;
            if (!IsTracked)
            {
                // Sin seguimiento no hay contactos: evita vibraciones "fantasma".
                SetAllActive(false);
                return;
            }

            for (int i = 0; i < FingerExtensions.Count; i++)
            {
                var sensor = m_Sensors[i];
                bool wanted = (enabledFingers & ((Finger)i).ToMask()) != 0;
                if (wanted && hand.GetJoint(k_TipJoints[i]).TryGetPose(out Pose pose))
                {
                    sensor.transform.SetLocalPositionAndRotation(pose.position, pose.rotation);
                    if (!sensor.gameObject.activeSelf) sensor.gameObject.SetActive(true);
                }
                else if (sensor.gameObject.activeSelf)
                {
                    sensor.gameObject.SetActive(false);
                }
            }
        }

        void SetAllActive(bool active)
        {
            foreach (var s in m_Sensors)
                if (s != null && s.gameObject.activeSelf != active) s.gameObject.SetActive(active);
        }

        void OnDrawGizmos()
        {
            if (!showGizmos) return;
            Gizmos.color = new Color(0.2f, 0.8f, 1f, 0.6f);
            foreach (var s in m_Sensors)
                if (s != null && s.gameObject.activeInHierarchy)
                    Gizmos.DrawWireSphere(s.transform.position, radius);
        }
    }
}
