# Cableado

```
                    3V3 ─────────────┬───────────────┬──────── … ───┐
                    GND ─────────────┼──┬────────────┼──┬───── … ───┼──┐
                                     │  │            │  │           │  │
 ┌────────────────────┐        ┌─────┴──┴────┐  ┌────┴──┴───┐  ┌────┴──┴───┐
 │ Arduino Nano       │  SDA   │  TCA9548A   │  │ DRV2605L  │  │ DRV2605L  │
 │ RP2040 Connect     ├─A4─────┤ SDA     SD0 ├──┤ SDA       │  │           │
 │                    ├─A5─────┤ SCL     SC0 ├──┤ SCL  OUT± ├─ LRA pulgar │
 │  (Wi‑Fi NINA‑W102) │  SCL   │  A0 A1 A2   │  └───────────┘  │           │
 └────────────────────┘        │  GND GND GND│        …        │           │
                               │         SD4 ├─────────────────┤ SDA       │
                               │         SC4 ├─────────────────┤ SCL  OUT± ├─ LRA meñique
                               └─────────────┘                 └───────────┘
```

| Desde | Hacia | Nota |
|---|---|---|
| Nano A4 (SDA) | TCA9548A SDA | bus principal a 400 kHz |
| Nano A5 (SCL) | TCA9548A SCL | |
| Nano 3V3 / GND | TCA9548A VIN / GND | |
| TCA9548A A0, A1, A2 | GND | dirección 0x70 |
| TCA9548A SDn / SCn | SDA / SCL del DRV2605L del dedo | canal según `FINGER_TO_MUX_CHANNEL` en `config.h` |
| DRV2605L OUT+ / OUT− | terminales del LRA | sin polaridad crítica en LRA |

Asignación por defecto de canales (se cambia en `firmware/guante_haptico/config.h`):

| Dedo | Índice lógico | Canal TCA9548A |
|---|---|---|
| Pulgar | 0 | 0 |
| Índice | 1 | 1 |
| Medio | 2 | 2 |
| Anular | 3 | 3 |
| Meñique | 4 | 4 |

El prototipo de la tesis base solo tenía índice, medio y anular. No hace falta cambiar nada: el firmware detecta al arrancar en qué canales hay un DRV2605L y los demás dedos se ignoran.

## Alimentación (revisar antes de soldar)

- **Niveles lógicos.** El RP2040 trabaja a 3,3 V y sus pines **no toleran 5 V**. Si las placas DRV2605L tienen resistencias pull‑up conectadas a su VIN y las alimentas a 5 V, el bus I2C quedaría a 5 V. Alimenta TCA9548A y DRV2605L a 3,3 V, o comprueba en el esquemático de tu placa a dónde van las pull‑ups.
- **Corriente.** Cada LRA consume del orden de 60–100 mA durante el *overdrive*. Cinco a la vez pueden superar lo que conviene sacar del pin 3V3 del Nano. Si se ven reinicios o caídas de Wi‑Fi al vibrar todos los dedos, usa un regulador de 3,3 V aparte desde la batería, con la masa común.
- Pon un condensador de 10 µF + 100 nF cerca de cada DRV2605L.

## Primera prueba

1. Sube `firmware/i2c_scanner/i2c_scanner.ino`.
2. En el Monitor Serie (115200) debe aparecer `0x5A` en cada canal que tenga driver.
3. Si no aparece nada, revisa SDA/SCL (A4/A5), masa común y que A0–A2 del TCA9548A estén a GND.
