#pragma once
#include "Arduino.h"
#include "IPAddress.h"
#include <deque>
#include <vector>
struct Datagram { std::vector<uint8_t> data; IPAddress ip; uint16_t port; };
class WiFiUDP {
 public:
  std::deque<Datagram> inbox;
  std::vector<Datagram> sent;
  Datagram cur, building;
  uint16_t port = 0;
  uint8_t begin(uint16_t p) { port = p; return 1; }
  void stop() {}
  int parsePacket() { if (inbox.empty()) return 0; cur = inbox.front(); inbox.pop_front(); return (int)cur.data.size(); }
  int read(uint8_t* b, size_t n) { size_t k = cur.data.size() < n ? cur.data.size() : n; memcpy(b, cur.data.data(), k); return (int)k; }
  IPAddress remoteIP() { return cur.ip; }
  uint16_t remotePort() { return cur.port; }
  int beginPacket(const IPAddress& ip, uint16_t p) { building = Datagram{{}, ip, p}; return 1; }
  size_t write(const uint8_t* b, size_t n) { building.data.insert(building.data.end(), b, b + n); return n; }
  int endPacket() { sent.push_back(building); return 1; }
};
