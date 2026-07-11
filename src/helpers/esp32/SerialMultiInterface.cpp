#include "SerialMultiInterface.h"

#ifndef USB_ACTIVE_HOLD_MS
#define USB_ACTIVE_HOLD_MS 30000
#endif

void SerialMultiInterface::begin(Stream& serial, const char* blePrefix, char* bleName, uint32_t blePin, int wifiPort, bool enableBle) {
  usb.begin(serial);
  bleStarted = false;
  if (enableBle) {
    ble.begin(blePrefix, bleName, blePin);
    bleStarted = true;
  }
  pendingWifiPort = wifiPort;
}

void SerialMultiInterface::beginWiFi() {
  if (wifiStarted || pendingWifiPort <= 0) return;

  wifi.begin(pendingWifiPort);
  wifiStarted = true;
  if (_isEnabled) {
    wifi.enable();
  }
}

void SerialMultiInterface::enable() {
  if (_isEnabled) return;

  _isEnabled = true;
  activeTransport = TRANSPORT_NONE;
  usbActiveUntil = 0;
  usb.enable();
  if (bleStarted) {
    ble.enable();
  }
  if (wifiStarted) {
    wifi.enable();
  }
}

void SerialMultiInterface::disable() {
  _isEnabled = false;
  activeTransport = TRANSPORT_NONE;
  usbActiveUntil = 0;
  usb.disable();
  if (bleStarted) {
    ble.disable();
  }
  if (wifiStarted) {
    wifi.disable();
  }
}

BaseSerialInterface* SerialMultiInterface::activeInterface() {
  switch (activeTransport) {
    case TRANSPORT_USB:
      return millis() < usbActiveUntil ? &usb : nullptr;
    case TRANSPORT_BLE:
      return bleStarted && ble.isConnected() ? &ble : nullptr;
    case TRANSPORT_WIFI:
      return wifiStarted && wifi.isConnected() ? &wifi : nullptr;
    default:
      return nullptr;
  }
}

BaseSerialInterface* SerialMultiInterface::preferredWriteInterface() {
  BaseSerialInterface* active = activeInterface();
  if (active != nullptr) return active;
  if (bleStarted && ble.isConnected()) return &ble;
  if (wifiStarted && wifi.isConnected()) return &wifi;
  if (millis() < usbActiveUntil) return &usb;
  return nullptr;
}

size_t SerialMultiInterface::pollTransport(BaseSerialInterface& iface, Transport transport, uint8_t dest[]) {
  size_t len = iface.checkRecvFrame(dest);
  if (len > 0) {
    activeTransport = transport;
    if (transport == TRANSPORT_USB) {
      usbActiveUntil = millis() + USB_ACTIVE_HOLD_MS;
    }
  }
  return len;
}

bool SerialMultiInterface::isConnected() const {
  return (bleStarted && ble.isConnected()) || (wifiStarted && wifi.isConnected()) || millis() < usbActiveUntil;
}

bool SerialMultiInterface::isWriteBusy() const {
  BaseSerialInterface* iface = const_cast<SerialMultiInterface*>(this)->activeInterface();
  return iface != nullptr && iface->isWriteBusy();
}

size_t SerialMultiInterface::writeFrame(const uint8_t src[], size_t len) {
  BaseSerialInterface* iface = preferredWriteInterface();
  return iface != nullptr ? iface->writeFrame(src, len) : 0;
}

size_t SerialMultiInterface::checkRecvFrame(uint8_t dest[]) {
  if (activeTransport == TRANSPORT_BLE && bleStarted) {
    size_t len = pollTransport(ble, TRANSPORT_BLE, dest);
    if (len > 0) return len;
  } else if (activeTransport == TRANSPORT_WIFI && wifiStarted) {
    size_t len = pollTransport(wifi, TRANSPORT_WIFI, dest);
    if (len > 0) return len;
  } else if (activeTransport == TRANSPORT_USB) {
    size_t len = pollTransport(usb, TRANSPORT_USB, dest);
    if (len > 0) return len;
  }

  size_t len = 0;
  if (bleStarted) {
    len = pollTransport(ble, TRANSPORT_BLE, dest);
    if (len > 0) return len;
  }
  if (wifiStarted) {
    len = pollTransport(wifi, TRANSPORT_WIFI, dest);
    if (len > 0) return len;
  }
  return pollTransport(usb, TRANSPORT_USB, dest);
}

const char* SerialMultiInterface::getActiveTransportName() const {
  switch (activeTransport) {
    case TRANSPORT_USB:
      return millis() < usbActiveUntil ? "USB" : "none";
    case TRANSPORT_BLE:
      return bleStarted && ble.isConnected() ? "BLE" : "none";
    case TRANSPORT_WIFI:
      return wifiStarted && wifi.isConnected() ? "WiFi" : "none";
    default:
      return "none";
  }
}
