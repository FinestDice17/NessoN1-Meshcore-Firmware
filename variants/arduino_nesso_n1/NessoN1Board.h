#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>
#include "NessoExpander.h"

class NessoN1Board : public ESP32Board {
  uint16_t lastBatteryMilliVolts = 0;
  uint16_t bootBatteryMilliVolts = 0;
  unsigned long bootMillis = 0;
  unsigned long lastBatteryReadMillis = 0;
  bool expanderReady = false;

  uint16_t readBatteryMilliVolts();
  bool isOtaButtonHeld();

public:
  void begin();
  void onBeforeTransmit() override;
  void onAfterTransmit() override;
  void powerOff() override;
  bool startOTAUpdate(const char* id, char reply[]) override;
  bool formatBoardDiagnostics(char* reply, size_t max_len) override;
  bool isExternalPowered() override;
  bool isKey1Held();
  bool isKey2Held();
  uint16_t getBootVoltage() override { return bootBatteryMilliVolts; }
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override;
};
