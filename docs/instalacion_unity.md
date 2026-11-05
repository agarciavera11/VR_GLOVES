# Instalación del proyecto Unity

Los scripts están en `unity/Assets/GuanteHaptico/`. No se suben `Library/` ni la configuración generada: se crea el proyecto y se copian los scripts.

## 1. Crear el proyecto

1. Unity Hub → **Unity 6** (6000.x LTS) → plantilla **Universal 3D**. Módulo **Android Build Support** instalado si se va a compilar para el visor.
2. Copia la carpeta `unity/Assets/GuanteHaptico` dentro de `Assets/` del proyecto nuevo.

## 2. Paquetes (Window → Package Manager)

| Paquete | Para qué |
|---|---|
| XR Plugin Management | gestión de XR |
| OpenXR Plugin | runtime XR |
| XR Interaction Toolkit (3.x) | rig del jugador, simulador de dispositivos XR |
| XR Hands | articulaciones de la mano (`UnityEngine.XR.Hands`) |
| Input System | teclado de pruebas (ya viene en Unity 6) |
| Unity OpenXR: Meta | funciones de Quest; se usará también para el passthrough en la fase de RA |

Importa también el ejemplo **Hand Visualizer** de XR Hands para ver la mano.

## 3. Configuración XR

**Project Settings → XR Plug-in Management**

- Pestaña Android: activa **OpenXR** y el grupo **Meta Quest**.
- Pestaña PC (para Quest Link / PCVR): activa **OpenXR**.

**OpenXR → Features** (en ambas pestañas):

- **Hand Tracking Subsystem**: activado.
- **Meta Hand Tracking Aim**: activado.
- Interaction profile: *Oculus Touch Controller Profile* (por si se usan mandos).

**Project Settings → Player → Android → Other Settings**

- **Internet Access: Require** (sin esto el visor no abre sockets UDP).
- Minimum API level: el que pida el paquete de Meta.

**Project Settings → Time**

- **Fixed Timestep = 0.01111** (90 Hz). Con el valor por defecto (0.02 s) el contacto puede detectarse hasta 20 ms tarde, que es más de la mitad del objetivo de 33 ms.

## 4. Escena mínima

1. Añade un **XR Origin (XR Rig)** (GameObject → XR → XR Origin).
2. Crea un objeto vacío `Guante` y añade:
   - `HapticGlove` (deja **Device Ip** vacío para descubrimiento automático, o escribe la IP que muestra el Monitor Serie).
   - `FingertipColliders` con **Handedness = Right**. **Enabled Fingers** = Índice, Medio, Anular si el prototipo solo tiene esos tres.
   - `HapticKeyboardTester` (opcional, para PC).
   - `LatencyProbe` (opcional).
3. Crea un cubo y añade `HapticSurface`. Al tocarlo con la mano debe vibrar el dedo que lo toca.
4. Crea una esfera pequeña con `PulseZone` (por ejemplo 60 ppm, patrón Normal) para la palpación.
5. Opcional: un objeto con `HapticStatusDisplay` delante de la cámara para ver el estado y la latencia dentro del visor.

## 5. Probar sin visor ni guante

1. En el PC: `python tools/emulador_guante.py` (simula el guante en el puerto 8888).
2. En Unity, en `HapticGlove` pon **Device Ip = 127.0.0.1** y dale a Play.
3. Pulsa 1–5, Q, W, S en el teclado: el emulador imprime lo que haría cada dedo.
4. Para simular manos sin visor, usa el **XR Device Simulator** del XR Interaction Toolkit (modo manos).

> El emulador y Unity no pueden usar el puerto 8889 a la vez con `guante_cli.py`. Usa uno u otro.

## 6. Pruebas automáticas

Window → General → **Test Runner** → EditMode → Run All. Las pruebas de `HapticProtocolTests` comprueban que Unity produce los mismos bytes que el firmware.

## Problemas comunes

| Síntoma | Causa probable |
|---|---|
| «Esperando anuncio del guante…» y nunca lo encuentra | el router bloquea broadcast, o el PC y el guante están en redes distintas. Escribe la IP a mano. En Windows, permite Unity en el firewall (UDP 8889). |
| Funciona en el Editor pero no en el visor | falta **Internet Access: Require**. |
| No aparecen las esferas de las yemas | Hand Tracking Subsystem desactivado, o las manos no se están siguiendo (iluminación, mandos en la mano). |
| Vibra con retraso irregular | Wi‑Fi saturado o lejos del router. Mide con `LatencyProbe` y revisa el RSSI en el estado. |
