// guante_haptico.ino — Firmware del guante háptico (Arduino Nano RP2040 Connect)
//
// Hardware: Nano RP2040 Connect -> I2C (A4 SDA, A5 SCL) -> TCA9548A (0x70)
//           -> un DRV2605L (0x5A) por canal -> un LRA por dedo.
// Comunicación: Wi-Fi 2,4 GHz + UDP (protocolo binario, docs/protocolo.md)
//               y consola de texto por USB para pruebas.
//
// Dependencias (Gestor de librerías): WiFiNINA. Placa: "Arduino Mbed OS Nano Boards".
// Antes de compilar: copia secrets.example.h como secrets.h.

#include <Arduino.h>
#include <Wire.h>

#include "command_handler.h"
#include "config.h"
#include "haptic_engine.h"
#include "net_link.h"
#include "serial_console.h"
#include "tca9548a.h"

Tca9548a mux(TCA9548A_ADDR);
HapticEngine engine(Wire, mux);
CommandHandler handler(engine);
NetLink net(handler);
SerialConsole console(Serial, engine, mux, net);

void setup() {
  Serial.begin(SERIAL_BAUD);
  // Espera breve al monitor serie, sin bloquear si el guante va sin USB.
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) {}

  Serial.println(F("\n== Guante haptico =="));
  Wire.begin();
  Wire.setClock(I2C_CLOCK_HZ);

  if (!mux.begin(Wire)) Serial.println(F("[i2c] ERROR: TCA9548A no responde en 0x70"));
  engine.begin(&Serial);
  net.begin(&Serial);
  console.printHelp();

  // Señal de arranque: un clic corto en todos los dedos presentes.
  engine.playEffect(0xFF, 1);
}

void loop() {
  const uint32_t now = millis();
  net.poll();               // primero la red: es lo sensible a latencia
  engine.update(now);
  console.poll(now);
  net.maintain(now, &Serial);
}
