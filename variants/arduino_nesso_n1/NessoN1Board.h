#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>
#include "NessoExpander.h"

class NessoN1Board : public ESP32Board {
  uint16_t lastBatteryMilliVolts = 0;

  uint16_t readBatteryMilliVolts();

public:
  void begin();
  void onBeforeTransmit() override;
  void onAfterTransmit() override;
  void powerOff() override;
  bool isExternalPowered() override;
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override;
};
