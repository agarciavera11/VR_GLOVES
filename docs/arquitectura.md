# Arquitectura

```
 Quest 3S (seguimiento óptico de manos)
        │ XR Hands: articulaciones *Tip de cada dedo
        ▼
 FingertipColliders ── crea 5 esferas trigger con Rigidbody cinemático
        │ física de Unity (OnTriggerEnter / Exit)
        ▼
 FingertipSensor ──► HapticSurface / PulseZone (deciden qué sentir)
        │ HapticGlove.Pulse / PlayEffect / Stop
        ▼
 HapticGlove ── UdpHapticTransport (hilo de recepción + cola)
        │ UDP 8888, paquete binario de 7–31 bytes
        ▼ Wi‑Fi 2,4 GHz
 Nano RP2040 Connect
   NetLink (WiFiNINA) ──► CommandHandler ──► HapticEngine
                                               │ Drv2605l (selección de canal en caché)
                                               ▼ I2C 400 kHz
                                          TCA9548A ──► 5 × DRV2605L ──► 5 × LRA
```

## Firmware (`firmware/guante_haptico/`)

| Archivo | Responsabilidad |
|---|---|
| `config.h` | todo lo ajustable: mapa dedo→canal, datos del LRA, puertos, tiempos |
| `protocol.*` | codificar y validar paquetes. C++ puro, se prueba en el PC |
| `tca9548a.*` | selección de canal del multiplexor. Recuerda el canal activo y no reescribe si no cambia |
| `drv2605l.*` | driver propio del DRV2605L para LRA en lazo cerrado: configuración, autocalibración no bloqueante, efectos, RTP |
| `haptic_engine.*` | los 5 dedos: detección, calibración en paralelo, temporización de pulsos sin `delay()` |
| `command_handler.*` | paquete → acción → respuesta. Independiente del transporte |
| `net_link.*` | Wi‑Fi, UDP, anuncio por broadcast, reconexión |
| `serial_console.*` | consola USB para pruebas; conserva los comandos de la tesis base (`v<n>`, `v<n>a`, `v<n>b`, `scan`) |

Decisiones que afectan a la latencia:

- **Sin `delay()` en el bucle.** Los pulsos se terminan en `HapticEngine::update()` comparando con `millis()`.
- **Sin eco por serie en cada comando.** En la tesis base, cada comando imprimía texto a 9600 baudios, lo que añade milisegundos. Ahora el registro solo sale por la consola cuando se pide.
- **Caché de canal y de modo.** Un pulso repetido en el mismo dedo cuesta **una** transacción I2C (comprobado en las pruebas del PC); el primero cuesta tres.
- **`WiFi.noLowPowerMode()`.** El ahorro de energía del módulo NINA retrasa la recepción.
- **Driver propio en vez de la librería de Adafruit**, para controlar cuántas escrituras hace cada comando y poder usar RTP con lazo cerrado y autocalibración.

## Unity (`unity/Assets/GuanteHaptico/`)

| Script | Responsabilidad |
|---|---|
| `HapticGlove` | API pública (`Pulse`, `PlayEffect`, `PlaySequence`, `Stop`, `Ping`), descubrimiento, estado |
| `UdpHapticTransport` | socket UDP, hilo de recepción, marcas de tiempo con `Stopwatch` |
| `HapticProtocol` | espejo del protocolo del firmware |
| `FingertipColliders` | esferas de contacto en las yemas a partir de XR Hands; se desactivan si se pierde el seguimiento |
| `FingertipSensor` | contacto y velocidad de cada yema |
| `HapticSurface` | impacto (efecto o pulso según la velocidad) y contacto sostenido |
| `PulseZone` | latido a 30–180 ppm, patrones normal / hipotenso / hipertenso |
| `LatencyProbe` | RTT con PING o PULSE+ACK, percentiles y CSV |
| `HapticKeyboardTester`, `HapticStatusDisplay` | pruebas y estado visible dentro del visor |

## Presupuesto de latencia (colisión → vibración)

| Etapa | Qué la determina | Cómo se mide |
|---|---|---|
| Detección del contacto | paso de física de Unity. **Por defecto es 20 ms (50 Hz)**; con 90 Hz baja a ~11 ms | `Time.fixedDeltaTime` |
| Envío UDP | < 1 ms | — |
| Wi‑Fi ida | variable, depende del router y de la interferencia | RTT/2 con `LatencyProbe` o `guante_cli.py latencia` |
| Firmware (red + I2C) | decenas a cientos de µs | campo «µs de proceso» del ACK; comando `bench` |
| Arranque mecánico del LRA | 32–50 ms según la tesis base; el *overdrive* y el *startup boost* lo acortan | medición física (acelerómetro u osciloscopio) |

La etapa mecánica del LRA es probablemente la más grande, y no se ve con mediciones de red. Por eso el plan de pruebas incluye una medición física.
