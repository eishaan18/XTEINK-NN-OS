#pragma once
enum { WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum { WIFI_MODE_NULL = 0 };
struct WiFiStub {
  int status() const { return WL_DISCONNECTED; }
  int getMode() const { return WIFI_MODE_NULL; }
  void disconnect(bool) {}
};
inline WiFiStub WiFi;
