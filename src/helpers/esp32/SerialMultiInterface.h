#pragma once

#include "../ArduinoSerialInterface.h"
#include "SerialBLEInterface.h"
#include "SerialWifiInterface.h"

class SerialMultiInterface : public BaseSerialInterface {
public:
  enum Transport : uint8_t {
    TRANSPORT_NONE = 0,
    TRANSPORT_USB,
    TRANSPORT_BLE,
    TRANSPORT_WIFI,
  };

private:
  ArduinoSerialInterface usb;
  SerialBLEInterface ble;
  SerialWifiInterface wifi;
  Transport activeTransport;
  unsigned long usbActiveUntil;
  int pendingWifiPort;
  bool wifiStarted;
  bool bleStarted;
  bool _isEnabled;

  BaseSerialInterface* activeInterface();
  BaseSerialInterface* preferredWriteInterface();
  size_t pollTransport(BaseSerialInterface& iface, Transport transport, uint8_t dest[]);

public:
  SerialMultiInterface() : activeTransport(TRANSPORT_NONE), usbActiveUntil(0), pendingWifiPort(0), wifiStarted(false), bleStarted(false), _isEnabled(false) {}

  void begin(Stream& serial, const char* blePrefix, char* bleName, uint32_t blePin, int wifiPort, bool enableBle = true);
  void beginWiFi();
  const char* getActiveTransportName() const;
  bool isBleStarted() const { return bleStarted; }
  bool isBleConnected() const { return bleStarted && ble.isConnected(); }
  bool isWifiConnected() const { return wifiStarted && wifi.isConnected(); }

  void enable() override;
  void disable() override;
  bool isEnabled() const override { return _isEnabled; }
  bool isConnected() const override;
  bool isWriteBusy() const override;
  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;
};
