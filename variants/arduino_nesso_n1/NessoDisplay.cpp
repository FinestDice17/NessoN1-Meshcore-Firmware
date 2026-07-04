#include "NessoDisplay.h"

NessoPanel::NessoPanel() {
  {
    auto cfg = bus.config();
    cfg.pin_mosi = P_LORA_MOSI;
    cfg.pin_miso = P_LORA_MISO;
    cfg.pin_sclk = P_LORA_SCLK;
    cfg.pin_dc = P_LCD_DC;
    cfg.freq_write = 40000000;
    bus.config(cfg);
    panel.setBus(&bus);
  }
  {
    auto cfg = panel.config();
    cfg.invert = true;
    cfg.pin_cs = P_LCD_CS;
    cfg.pin_rst = -1;
    cfg.pin_busy = -1;
    cfg.panel_width = 135;
    cfg.panel_height = 240;
    cfg.offset_x = 52;
    cfg.offset_y = 40;
    panel.config(cfg);
  }
  setPanel(&panel);
}

bool NessoMeshDisplay::begin() {
  nessoExpander.pinMode(NESSO_LCD_BACKLIGHT, OUTPUT);
  nessoExpander.pinMode(NESSO_LCD_RESET, OUTPUT);

  nessoExpander.digitalWrite(NESSO_LCD_BACKLIGHT, LOW);
  nessoExpander.digitalWrite(NESSO_LCD_RESET, LOW);
  delay(100);
  nessoExpander.digitalWrite(NESSO_LCD_RESET, HIGH);
  delay(20);

  display->init();
  display->setRotation(1);
  display->setColorDepth(8);
  display->setTextColor(TFT_WHITE);

  buffer.setColorDepth(8);
  buffer.setPsram(false);
  if (!buffer.createSprite(width(), height())) {
    return false;
  }

  nessoExpander.digitalWrite(NESSO_LCD_BACKLIGHT, HIGH);
  _isOn = true;
  return true;
}

void NessoMeshDisplay::turnOn() {
  if (!_isOn) {
    display->wakeup();
    nessoExpander.digitalWrite(NESSO_LCD_BACKLIGHT, HIGH);
    _isOn = true;
  }
}

void NessoMeshDisplay::turnOff() {
  if (_isOn) {
    nessoExpander.digitalWrite(NESSO_LCD_BACKLIGHT, LOW);
    display->sleep();
    _isOn = false;
  }
}
