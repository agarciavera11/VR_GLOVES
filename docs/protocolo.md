# Protocolo Unity ↔ guante (v1)

Transporte principal: **UDP sobre Wi‑Fi 2,4 GHz**. El guante escucha en el puerto **8888**; Unity (o `tools/guante_cli.py`) usa un único socket en el puerto **8889**, desde el que envía comandos y en el que recibe respuestas y anuncios.

Se eligió UDP y no TCP porque un comando háptico que llega tarde ya no sirve: con TCP, un paquete perdido bloquea a todos los siguientes hasta su retransmisión (*head‑of‑line blocking*). Con UDP cada comando es independiente y el pulso tiene duración propia, así que un paquete perdido solo pierde una vibración.

## Formato del paquete

Todos los campos multibyte van en *little‑endian*.

| Byte | Campo | Descripción |
|---|---|---|
| 0 | `MAGIC` | `0xA7` |
| 1 | `VERSION` | `1` |
| 2 | `TYPE` | tipo de paquete (tabla siguiente) |
| 3 | `FLAGS` | bit 0 = `ACK_REQUEST` (el guante responde `ACK`) |
| 4–5 | `SEQ` | número de secuencia u16; las respuestas repiten el `SEQ` del comando |
| 6… | `PAYLOAD` | 0–24 bytes, según el tipo |
| último | `CRC` | CRC‑8 (polinomio 0x07, valor inicial 0x00) de todos los bytes anteriores |

Tamaño máximo: 31 bytes. Un pulso ocupa 11 bytes.

## Tipos

### Host → guante

| Tipo | Código | Payload | Respuesta |
|---|---|---|---|
| `PING` | `0x01` | u32 eco (marca de tiempo del host) | `PONG` |
| `EFFECT` | `0x10` | u8 máscara de dedos, u8 id de efecto (1–123, librería LRA de TI) | `ACK` si se pide |
| `PULSE` | `0x11` | u8 máscara, u8 amplitud (0–255), u16 duración ms (máx. 2000) | `ACK` si se pide |
| `SEQUENCE` | `0x12` | u8 máscara, 1–8 × u8 id de efecto | `ACK` si se pide |
| `STOP` | `0x13` | u8 máscara | `ACK` si se pide |
| `STATUS_REQ` | `0x20` | vacío | `STATUS` |

### Guante → host

| Tipo | Código | Payload |
|---|---|---|
| `PONG` | `0x81` | u32 eco, u32 `micros()` del guante al recibir, u16 µs de proceso |
| `ACK` | `0x82` | u16 µs de proceso (desde la recepción hasta terminar las escrituras I2C) |
| `STATUS` | `0xA0` | ver abajo (16 bytes) |
| `ANNOUNCE` | `0xA1` | igual que `STATUS`; se envía por broadcast a `255.255.255.255:8889` cada 1 s mientras no hay host |
| `ERROR` | `0xEE` | u8 código (`ParseResult`: 3 versión, 4 CRC, 5 longitud, 6 tipo) |

Payload de `STATUS`/`ANNOUNCE`:

| Offset | Tipo | Campo |
|---|---|---|
| 0 | u8 | versión de firmware (mayor) |
| 1 | u8 | versión de firmware (menor) |
| 2 | u8 | número de dedos (5) |
| 3 | u8 | máscara de dedos con DRV2605L detectado |
| 4 | u8 | máscara de dedos con autocalibración LRA correcta |
| 5 | i8 | RSSI en dBm |
| 6 | u32 | tiempo encendido (ms) |
| 10 | u32 | paquetes válidos recibidos |
| 14 | u16 | paquetes con error |

## Máscara de dedos

`bit0` pulgar, `bit1` índice, `bit2` medio, `bit3` anular, `bit4` meñique. `0x1F` = todos. Un mismo paquete puede activar varios dedos a la vez, que es más rápido que mandar un paquete por dedo.

## Sesión y descubrimiento

1. Al arrancar, el guante se conecta al Wi‑Fi y emite `ANNOUNCE` por broadcast cada segundo.
2. Unity recibe el anuncio, toma la IP de origen y empieza a enviar comandos al puerto 8888. Si la red bloquea el broadcast, la IP se puede escribir a mano en el componente `HapticGlove`.
3. Unity pide `STATUS` cada segundo. Mientras llega tráfico, el guante no se anuncia; si pasan 3 s sin paquetes, vuelve a anunciarse.

## Seguridad

- Ningún `PULSE` dura más de 2000 ms, aunque se pida más.
- Los pulsos terminan solos en el firmware: si se pierde la conexión, el guante no se queda vibrando.
- Los paquetes con otro `MAGIC` se descartan sin respuesta (tráfico ajeno en la red).

## Medición de latencia

- `PING`/`PONG` mide el tiempo de ida y vuelta (RTT) de la red más el bucle del firmware.
- `PULSE` con `ACK_REQUEST` mide además la ejecución I2C real en el guante.
- El campo de µs de proceso separa el tiempo del firmware del tiempo de red.
- La latencia de ida se estima como RTT/2. Para la latencia total colisión→vibración hace falta además una medición física (ver `docs/pruebas.md`).

## Vectores de referencia

`docs/golden_vectors.txt` contiene paquetes generados por el firmware compilado en el PC. Las pruebas de Python (`tools/test_herramientas.py`) y de Unity (`Tests/Editor/HapticProtocolTests.cs`) comprueban esos mismos bytes, de modo que las tres implementaciones no pueden desincronizarse sin que falle una prueba.
