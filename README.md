# VR Gloves: guante háptico para Unity (VR → RA)

Firmware, herramientas y scripts de Unity para un guante con retroalimentación háptica por dedo, pensado para el Meta Quest 3S con seguimiento óptico de manos.

Proyecto de Itinerario de Investigación (ESPOL, FIMCP), continuación de la tesis de J. Mejía Parra (2025). Este repositorio reconstruye el sistema desde cero a partir de lo que se conoce del hardware.

## Hardware

- Arduino Nano RP2040 Connect (Wi‑Fi 2,4 GHz por el módulo u‑blox NINA‑W102)
- Multiplexor I2C TCA9548A (0x70)
- Un DRV2605L (0x5A) por dedo, hasta 5
- Actuadores LRA tipo moneda (200–260 Hz)
- Meta Quest 3S

## Estructura

```
firmware/
  guante_haptico/      firmware principal (sketch de Arduino)
  i2c_scanner/         prueba de cableado del TCA9548A
  test_host/           compilación y pruebas del firmware en el PC
unity/Assets/GuanteHaptico/
  Runtime/             HapticGlove, transporte UDP, yemas con XR Hands, superficies, diagnóstico
  Tests/Editor/        pruebas del protocolo
tools/
  guante_cli.py        comandos y medición de latencia desde el PC
  emulador_guante.py   simula el guante (desarrollar Unity sin hardware)
docs/                  protocolo, cableado, arquitectura, instalación de Unity, pruebas
```

## Inicio rápido

**Firmware**

1. Arduino IDE → Gestor de placas → *Arduino Mbed OS Nano Boards*. Gestor de librerías → *WiFiNINA*.
2. Copia `firmware/guante_haptico/secrets.example.h` como `secrets.h` y escribe tu red Wi‑Fi (2,4 GHz).
3. Revisa `config.h` (mapa dedo→canal y datos del LRA).
4. Sube `guante_haptico.ino`. Abre el Monitor Serie a 115200 y escribe `help`.

**PC**

```bash
python tools/guante_cli.py descubrir
python tools/guante_cli.py --ip 192.168.1.50 pulso indice 200 150
python tools/guante_cli.py --ip 192.168.1.50 latencia -n 500 --csv latencia.csv
```

**Unity**: ver [docs/instalacion_unity.md](docs/instalacion_unity.md).

## Pruebas

```bash
make -C firmware/test_host          # firmware compilado en el PC con I2C/Wi‑Fi simulados
python tools/test_herramientas.py   # protocolo + emulador
```

## Documentación

- [Protocolo](docs/protocolo.md)
- [Cableado](docs/cableado.md)
- [Arquitectura y presupuesto de latencia](docs/arquitectura.md)
- [Instalación de Unity](docs/instalacion_unity.md)
- [Plan de pruebas](docs/pruebas.md)

## Estado

| Parte | Estado |
|---|---|
| Firmware (I2C, DRV2605L, Wi‑Fi/UDP, consola) | escrito y probado en el PC con hardware simulado; falta probarlo en la placa |
| Protocolo v1 | cerrado; Python, C++ y C# comparten vectores de prueba |
| Unity (XR Hands, superficies, pulso, latencia) | escrito; falta montarlo en un proyecto Unity 6 y probarlo en el visor |
| Migración a RA (passthrough) | siguiente fase |
