#pragma once

#include <helpers/ui/DisplayDriver.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#ifndef UI_ZOOM
  #define UI_ZOOM 1
#endif

class LGFXDisplay : public DisplayDriver {
protected:
  LGFX_Device* display;
  LGFX_Sprite buffer;

  bool _isOn = false;
  int _color = TFT_WHITE;
  int _contentRotation = 0;  // degrees; applied when compositing the sprite in endFrame()

public:
  LGFXDisplay(int w, int h, LGFX_Device &disp)
    : DisplayDriver(w/UI_ZOOM, h/UI_ZOOM), display(&disp) {}
  bool begin();
  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
  virtual bool getTouch(int *x, int *y);

  // Rotate rendered content by the given angle (e.g. 180) when compositing
  // to the panel, without touching the panel's own native rotation/MADCTL
  // state. Useful for panels whose native rotation modes aren't correctly
  // calibrated (offsets, etc.) beyond their one default orientation.
  void setContentRotation(int degrees) { _contentRotation = degrees; }
};
