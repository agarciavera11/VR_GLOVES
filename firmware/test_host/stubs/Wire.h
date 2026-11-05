// Bus I2C simulado: TCA9548A en 0x70 y un DRV2605L (0x5A) en los canales habilitados.
#pragma once
#include "Arduino.h"
#include <vector>

struct FakeDrv {
  uint8_t reg[256] = {0};
  uint8_t ptr = 0;
  int goReadsUntilDone = 0;
  FakeDrv() { reg[0x00] = 0xE0; reg[0x01] = 0x40; }  // id 7 (DRV2605L), standby
};

class TwoWire {
 public:
  uint8_t muxMask = 0;
  bool channelPopulated[8] = {false};
  FakeDrv drv[8];
  std::vector<uint8_t> txBuf;
  uint8_t txAddr = 0;
  uint32_t transactions = 0;
  std::vector<uint8_t> rx;
  uint32_t clock = 0;

  void begin() {}
  void setClock(uint32_t c) { clock = c; }
  void beginTransmission(uint8_t a) { txAddr = a; txBuf.clear(); }
  size_t write(uint8_t b) { txBuf.push_back(b); return 1; }
  FakeDrv* target(uint8_t a) {
    if (a != 0x5A) return nullptr;
    FakeDrv* t = nullptr; int n = 0;
    for (int c = 0; c < 8; c++) if ((muxMask & (1 << c)) && channelPopulated[c]) { t = &drv[c]; n++; }
    return n == 1 ? t : nullptr;  // dos drivers con la misma dirección = conflicto
  }
  uint8_t endTransmission(bool stop = true) {
    (void)stop; transactions++;
    if (txAddr == 0x70) { if (!txBuf.empty()) muxMask = txBuf[0]; return 0; }
    FakeDrv* d = target(txAddr);
    if (!d) return 2;  // NACK
    if (txBuf.empty()) return 0;
    d->ptr = txBuf[0];
    for (size_t i = 1; i < txBuf.size(); i++) {
      uint8_t r = d->ptr++;
      d->reg[r] = txBuf[i];
      if (r == 0x0C && (txBuf[i] & 1) && (d->reg[0x01] & 7) == 7) d->goReadsUntilDone = 3;
    }
    return 0;
  }
  uint8_t requestFrom(uint8_t a, uint8_t n) {
    transactions++;
    FakeDrv* d = target(a);
    if (!d) return 0;
    rx.clear();
    for (uint8_t i = 0; i < n; i++) {
      uint8_t r = d->ptr++;
      uint8_t v = d->reg[r];
      if (r == 0x0C && d->goReadsUntilDone > 0) { if (--d->goReadsUntilDone == 0) d->reg[0x0C] = 0; v = d->reg[0x0C]; }
      rx.push_back(v);
    }
    return n;
  }
  int read() { if (rx.empty()) return -1; int v = rx.front(); rx.erase(rx.begin()); return v; }
};
extern TwoWire Wire;
