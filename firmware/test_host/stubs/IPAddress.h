#pragma once
#include "Arduino.h"
class IPAddress : public Printable {
 public:
  uint8_t b[4] = {0, 0, 0, 0};
  IPAddress() {}
  IPAddress(uint8_t a, uint8_t c, uint8_t d, uint8_t e) { b[0] = a; b[1] = c; b[2] = d; b[3] = e; }
  size_t printTo(Print& p) const override {
    char s[20];
    snprintf(s, sizeof s, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return p.print(s);
  }
};
