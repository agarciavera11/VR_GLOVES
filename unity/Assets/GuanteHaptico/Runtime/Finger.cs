using System;

namespace GuanteHaptico
{
    /// <summary>Índice lógico del dedo. Coincide con el firmware y el protocolo.</summary>
    public enum Finger : byte
    {
        Pulgar = 0,
        Indice = 1,
        Medio = 2,
        Anular = 3,
        Menique = 4,
    }

    /// <summary>Máscara de dedos (bit i = dedo i).</summary>
    [Flags]
    public enum FingerMask : byte
    {
        Ninguno = 0,
        Pulgar = 1 << 0,
        Indice = 1 << 1,
        Medio = 1 << 2,
        Anular = 1 << 3,
        Menique = 1 << 4,
        Todos = Pulgar | Indice | Medio | Anular | Menique,
    }

    public static class FingerExtensions
    {
        public const int Count = 5;
        public static FingerMask ToMask(this Finger f) => (FingerMask)(1 << (int)f);
    }
}
