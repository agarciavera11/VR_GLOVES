#pragma once
#include "Arduino.h"
#include "IPAddress.h"
#define WL_NO_MODULE 255
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
struct FakeWiFi {
  int st = WL_DISCONNECTED;
  bool lowPower = true;
  int status() { return st; }
  int begin(const char*, const char*) { st = WL_CONNECTED; return st; }
  const char* firmwareVersion() { return "1.5.0"; }
  void noLowPowerMode() { lowPower = false; }
  long RSSI() { return -55; }
  const char* localIP() { return "192.168.1.50"; }
};
extern FakeWiFi WiFi;
