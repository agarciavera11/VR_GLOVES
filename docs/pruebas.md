# Plan de pruebas

## Automáticas (sin hardware)

| Prueba | Comando | Qué comprueba |
|---|---|---|
| Firmware en PC | `make -C firmware/test_host` | protocolo, configuración de registros del DRV2605L, calibración, pulsos y fin automático, límite de 2 s, número de transacciones I2C, respuestas UDP, anuncio, consola serie (61 comprobaciones) |
| Herramientas Python | `python tools/test_herramientas.py` | protocolo frente a los vectores del firmware, emulador de extremo a extremo |
| Unity | Test Runner → EditMode | protocolo C# frente a los mismos vectores |

El firmware se compila contra *stubs* de Arduino (`firmware/test_host/stubs/`) que simulan el bus I2C con un TCA9548A y varios DRV2605L. La compilación real para la placa se hace con el IDE de Arduino o `arduino-cli`.

## Con hardware

1. **Cableado**: `i2c_scanner.ino` → `0x5A` en cada canal con driver.
2. **Calibración**: al arrancar el firmware, el Monitor Serie muestra `presentes=… calibrados=…`. Si un dedo presente no calibra, revisar el LRA y los valores `LRA_*` de `config.h`.
3. **Actuadores**: `v1`, `v1a`, `v1b`, `p a 200 300`, `e 2 47` en la consola.
4. **Tiempo de firmware**: comando `bench` (µs de un pulso en todos los dedos).
5. **Red**: `python tools/guante_cli.py descubrir`, luego `estado`.
6. **Latencia de red**: `python tools/guante_cli.py --ip <ip> latencia -n 1000 --csv ping.csv` y `... latencia --comando -n 1000 --csv pulso.csv`.
7. **Latencia en Unity**: `LatencyProbe` en la escena, desde el Editor (PCVR) y desde el visor (standalone).

## Latencia física colisión → vibración (pendiente de montaje)

La red no mide el arranque mecánico del LRA. Propuesta:

- Un acelerómetro (por ejemplo, un MPU‑6050 en un segundo microcontrolador) pegado sobre un LRA, o un micrófono de contacto.
- Unity hace parpadear un cuadro de la pantalla en el mismo frame en que manda el comando; un fotodiodo lo detecta.
- Se registran ambas señales en un osciloscopio o en el segundo microcontrolador, y la diferencia es la latencia total.

## Registro de resultados

Cada medición se guarda en CSV (`muestra, rtt_ms, proceso_us`) con fecha, distancia al router, RSSI y canal Wi‑Fi anotados aparte. Se informa media, p50, p95, p99, máximo y pérdidas.
