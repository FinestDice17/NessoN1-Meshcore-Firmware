#pragma once

#include <helpers/ui/LGFXDisplay.h>
#include "NessoExpander.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class NessoPanel : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel;
  lgfx::Bus_SPI bus;

public:
  NessoPanel();
};

class NessoMeshDisplay : public LGFXDisplay {
  NessoPanel panel;

public:
  NessoMeshDisplay() : LGFXDisplay(240, 135, panel) {}
  bool begin();
  void turnOn() override;
  void turnOff() override;
};
