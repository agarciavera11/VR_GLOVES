// config.h — Parámetros del guante. Ajusta esta sección a tu montaje.
#pragma once

#include <stdint.h>

#define FW_VERSION_MAJOR 0
#define FW_VERSION_MINOR 4

// ---------------------------------------------------------------- Dedos ----
// Índice lógico del dedo (igual que en Unity y en el protocolo):
//   0 = pulgar, 1 = índice, 2 = medio, 3 = anular, 4 = meñique
constexpr uint8_t FINGER_COUNT = 5;

// Canal del TCA9548A al que está conectado el DRV2605L de cada dedo.
// En la tesis base solo se usaban índice, medio y anular; el firmware detecta
// al arrancar qué canales tienen driver y simplemente ignora los que faltan.
// AJUSTAR según el cableado real.
constexpr uint8_t FINGER_TO_MUX_CHANNEL[FINGER_COUNT] = {0, 1, 2, 3, 4};

// ------------------------------------------------------------------ I2C ----
constexpr uint8_t TCA9548A_ADDR = 0x70;   // A0..A2 a GND
constexpr uint8_t DRV2605L_ADDR = 0x5A;   // fija, por eso hace falta el mux
constexpr uint32_t I2C_CLOCK_HZ = 400000; // ambos chips soportan Fast-mode

// -------------------------------------------------------------- LRA --------
// Valores del actuador para la autocalibración (DRV2605L, sección 8.5.6).
// Datos típicos de un LRA tipo moneda de 10 mm. AJUSTAR con la hoja de datos
// del LRA que tengas: tensión nominal RMS, tensión máxima y frecuencia.
constexpr float LRA_RATED_VRMS = 1.8f;   // V RMS nominal
constexpr float LRA_MAX_VPEAK = 2.5f;    // V pico máximo (overdrive clamp)
constexpr float LRA_FREQ_HZ = 235.0f;    // frecuencia de resonancia (200–260 Hz)
constexpr bool LRA_AUTOCALIBRATE = true; // si falla, se sigue en lazo cerrado sin calibrar

// ------------------------------------------------------------ Red ----------
constexpr uint16_t UDP_LISTEN_PORT = 8888;   // el guante escucha aquí
constexpr uint16_t UDP_ANNOUNCE_PORT = 8889; // Unity escucha aquí (anuncios/respuestas)
constexpr uint32_t ANNOUNCE_INTERVAL_MS = 1000;
constexpr uint32_t HOST_TIMEOUT_MS = 3000;   // sin tráfico => se vuelve a anunciar
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

// --------------------------------------------------------- Seguridad -------
constexpr uint16_t MAX_PULSE_MS = 2000;  // ningún pulso RTP dura más que esto

// ------------------------------------------------------------ Serie --------
constexpr uint32_t SERIAL_BAUD = 115200;

// Credenciales Wi-Fi: copia secrets.example.h como secrets.h (no se sube a git).
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "Falta secrets.h: copia secrets.example.h a secrets.h y pon tu red Wi-Fi"
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif
