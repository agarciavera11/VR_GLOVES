// i2c_scanner.ino — Prueba de cableado: recorre los 8 canales del TCA9548A y
// lista los dispositivos I2C de cada uno. Cada DRV2605L debe aparecer como 0x5A
// en su canal. Sube este sketch antes del firmware completo.
#include <Wire.h>

constexpr uint8_t TCA_ADDR = 0x70;

bool tcaSelect(uint8_t ch) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(1 << ch);
  return Wire.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Wire.begin();
  Wire.setClock(400000);
}

void loop() {
  Serial.println(F("\n--- Escaneo TCA9548A ---"));
  for (uint8_t ch = 0; ch < 8; ch++) {
    if (!tcaSelect(ch)) {
      Serial.println(F("ERROR: el TCA9548A no responde en 0x70 (revisa SDA=A4, SCL=A5, VIN, GND)"));
      break;
    }
    Serial.print(F("Canal "));
    Serial.print(ch);
    Serial.print(':');
    bool found = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
      if (addr == TCA_ADDR) continue;
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.print(F(" 0x"));
        Serial.print(addr, HEX);
        found = true;
      }
    }
    Serial.println(found ? F("") : F(" -"));
  }
  delay(5000);
}
