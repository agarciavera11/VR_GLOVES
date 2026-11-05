// Stub mínimo de Arduino para compilar y probar el firmware en el PC.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <string>

#define F(x) (x)
#define HEX 16
#define BIN 2
#define DEC 10
template <typename T, typename L, typename H>
inline T constrain(T x, L lo, H hi) { return x < (T)lo ? (T)lo : (x > (T)hi ? (T)hi : x); }

extern uint32_t g_millis, g_micros;
inline uint32_t millis() { return g_millis; }
inline uint32_t micros() { return g_micros; }
inline void delay(uint32_t ms) { g_millis += ms; g_micros += ms * 1000; }

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) { out += (char)c; return 1; }
  std::string out;
  size_t print(const char* s) { out += s; return strlen(s); }
  size_t print(const struct Printable& x);
  size_t print(char c) { out += c; return 1; }
  size_t print(long v, int base = DEC) {
    char b[40];
    if (base == HEX) snprintf(b, sizeof b, "%lX", v);
    else if (base == BIN) { std::string s; unsigned long u = (unsigned long)v; do { s.insert(s.begin(), (char)('0' + (u & 1))); u >>= 1; } while (u); out += s; return s.size(); }
    else snprintf(b, sizeof b, "%ld", v);
    out += b; return strlen(b);
  }
  size_t print(int v, int base = DEC) { return print((long)v, base); }
  size_t print(unsigned v, int base = DEC) { return print((long)v, base); }
  size_t print(unsigned long v, int base = DEC) { return print((long)v, base); }
  size_t print(uint8_t v, int base = DEC) { return print((long)v, base); }
  template <typename T> size_t println(T v) { size_t n = print(v); out += "\n"; return n + 1; }
  template <typename T> size_t println(T v, int base) { size_t n = print(v, base); out += "\n"; return n + 1; }
  size_t println() { out += "\n"; return 1; }
};

class Print;
struct Printable { virtual ~Printable() {} virtual size_t printTo(Print& p) const = 0; };
class Stream : public Print {
 public:
  std::string in;
  int available() { return (int)in.size(); }
  int read() { if (in.empty()) return -1; int c = (uint8_t)in[0]; in.erase(0, 1); return c; }
};

inline size_t Print::print(const Printable& x) { return x.printTo(*this); }
