#include "serial_console.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"

// Efectos de la librería LRA (TI, tabla 11.2) usados por los atajos de la tesis.
static constexpr uint8_t EFFECT_DEFAULT = 1;     // Strong Click 100 %
static constexpr uint8_t EFFECT_HIPOTENSO = 9;   // Soft Bump 30 %   (pulso débil)
static constexpr uint8_t EFFECT_HIPERTENSO = 10; // Double Click 100 % (pulso fuerte)

void SerialConsole::poll(uint32_t nowMs) {
  while (io_.available() > 0) {
    const int c = io_.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf_[len_] = '\0';
      if (len_ > 0) execute(buf_, nowMs);
      len_ = 0;
    } else if (len_ < sizeof(buf_) - 1) {
      buf_[len_++] = (char)c;
    }
  }
}

void SerialConsole::printHelp() {
  io_.println(F("Comandos:"));
  io_.println(F("  help                 esta ayuda"));
  io_.println(F("  scan                 lista canales del TCA9548A y dispositivos"));
  io_.println(F("  status               estado de dedos y Wi-Fi"));
  io_.println(F("  cal                  repite la autocalibracion LRA"));
  io_.println(F("  v<n> | v<n>a | v<n>b vibrar dedo n (normal / hipotenso / hipertenso)"));
  io_.println(F("  e <dedo|a> <id>      efecto de libreria (1..123)"));
  io_.println(F("  p <dedo|a> <amp> <ms> pulso RTP, amp 0..255"));
  io_.println(F("  s                    parar todo"));
  io_.println(F("  bench                mide el tiempo I2C de un comando"));
  io_.println(F("Dedos: 0 pulgar, 1 indice, 2 medio, 3 anular, 4 menique, a = todos"));
}

uint8_t SerialConsole::parseFingerMask(const char* tok) {
  if (tok == nullptr) return 0;
  if (tok[0] == 'a' && tok[1] == '\0') return (uint8_t)((1u << FINGER_COUNT) - 1);
  if (tok[0] >= '0' && tok[0] < (char)('0' + FINGER_COUNT) && tok[1] == '\0')
    return (uint8_t)(1u << (tok[0] - '0'));
  return 0;
}

void SerialConsole::execute(char* line, uint32_t nowMs) {
  // Compatibilidad con la tesis base: v<n>, v<n>a, v<n>b
  if (line[0] == 'v' && line[1] >= '0' && line[1] <= '9') {
    const uint8_t finger = (uint8_t)(line[1] - '0');
    if (finger >= FINGER_COUNT) {
      io_.println(F("ERR dedo fuera de rango"));
      return;
    }
    uint8_t effect = EFFECT_DEFAULT;
    if (line[2] == 'a') effect = EFFECT_HIPOTENSO;
    else if (line[2] == 'b') effect = EFFECT_HIPERTENSO;
    const uint8_t done = engine_.playEffect((uint8_t)(1u << finger), effect);
    io_.println(done ? F("OK") : F("ERR dedo sin driver"));
    return;
  }

  char* cmd = strtok(line, " ");
  if (cmd == nullptr) return;
  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
    printHelp();
  } else if (!strcmp(cmd, "scan")) {
    scan();
  } else if (!strcmp(cmd, "status")) {
    status();
  } else if (!strcmp(cmd, "cal")) {
    engine_.calibrate(&io_);
  } else if (!strcmp(cmd, "s")) {
    engine_.stop(0xFF);
    io_.println(F("OK"));
  } else if (!strcmp(cmd, "e")) {
    const uint8_t mask = parseFingerMask(strtok(nullptr, " "));
    const char* id = strtok(nullptr, " ");
    if (!mask || !id) {
      io_.println(F("uso: e <dedo|a> <id>"));
      return;
    }
    const uint8_t done = engine_.playEffect(mask, (uint8_t)atoi(id));
    io_.print(F("OK mascara=0b"));
    io_.println(done, BIN);
  } else if (!strcmp(cmd, "p")) {
    const uint8_t mask = parseFingerMask(strtok(nullptr, " "));
    const char* amp = strtok(nullptr, " ");
    const char* ms = strtok(nullptr, " ");
    if (!mask || !amp || !ms) {
      io_.println(F("uso: p <dedo|a> <amp 0..255> <ms>"));
      return;
    }
    const uint8_t done = engine_.pulse(mask, (uint8_t)constrain(atoi(amp), 0, 255),
                                       (uint16_t)constrain(atol(ms), 0L, 65535L), nowMs);
    io_.print(F("OK mascara=0b"));
    io_.println(done, BIN);
  } else if (!strcmp(cmd, "bench")) {
    bench();
  } else {
    io_.println(F("ERR comando desconocido (help)"));
  }
}

void SerialConsole::scan() {
  io_.println(F("Escaneo I2C por canal del TCA9548A:"));
  for (uint8_t ch = 0; ch < 8; ch++) {
    if (!mux_.select(ch)) {
      io_.println(F("  ERR: el TCA9548A no responde en 0x70"));
      return;
    }
    io_.print(F("  canal "));
    io_.print(ch);
    io_.print(':');
    bool any = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
      if (addr == TCA9548A_ADDR) continue;
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        io_.print(F(" 0x"));
        io_.print(addr, HEX);
        any = true;
      }
    }
    io_.println(any ? F("") : F(" (vacio)"));
  }
  mux_.disableAll();
}

void SerialConsole::status() {
  io_.print(F("fw "));
  io_.print(FW_VERSION_MAJOR);
  io_.print('.');
  io_.println(FW_VERSION_MINOR);
  io_.print(F("dedos presentes=0b"));
  io_.print(engine_.presentMask(), BIN);
  io_.print(F(" calibrados=0b"));
  io_.println(engine_.calibratedMask(), BIN);
  net_.printInfo(io_);
}

void SerialConsole::bench() {
  // Mide cuánto tarda el firmware en ejecutar un pulso en todos los dedos
  // presentes (selección de canal + escrituras I2C). No incluye la red.
  const uint8_t all = engine_.presentMask();
  if (!all) {
    io_.println(F("sin dedos presentes"));
    return;
  }
  uint32_t total = 0, worst = 0;
  const int N = 50;
  for (int i = 0; i < N; i++) {
    const uint32_t t0 = micros();
    engine_.pulse(all, 200, 5, millis());
    const uint32_t dt = micros() - t0;
    total += dt;
    if (dt > worst) worst = dt;
    delay(10);
    engine_.update(millis());
  }
  engine_.stop(0xFF);
  io_.print(F("pulso en mascara 0b"));
  io_.print(all, BIN);
  io_.print(F(": media="));
  io_.print(total / N);
  io_.print(F(" us, peor="));
  io_.print(worst);
  io_.println(F(" us"));
}
