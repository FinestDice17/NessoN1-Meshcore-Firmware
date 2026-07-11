#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <helpers/ui/MomentaryButton.h>

struct NessoExpanderPin {
  uint8_t address;
  uint8_t pin;
};

static constexpr uint8_t NESSO_EXPANDER_E0 = 0x43;
static constexpr uint8_t NESSO_EXPANDER_E1 = 0x44;

static constexpr NessoExpanderPin NESSO_KEY1 = {NESSO_EXPANDER_E0, 0};
static constexpr NessoExpanderPin NESSO_KEY2 = {NESSO_EXPANDER_E0, 1};
static constexpr NessoExpanderPin NESSO_LORA_LNA_ENABLE = {NESSO_EXPANDER_E0, 5};
static constexpr NessoExpanderPin NESSO_LORA_ANTENNA_SWITCH = {NESSO_EXPANDER_E0, 6};
static constexpr NessoExpanderPin NESSO_LORA_ENABLE = {NESSO_EXPANDER_E0, 7};

static constexpr NessoExpanderPin NESSO_POWEROFF = {NESSO_EXPANDER_E1, 0};
static constexpr NessoExpanderPin NESSO_LCD_RESET = {NESSO_EXPANDER_E1, 1};
static constexpr NessoExpanderPin NESSO_GROVE_POWER_EN = {NESSO_EXPANDER_E1, 2};
static constexpr NessoExpanderPin NESSO_VIN_DETECT = {NESSO_EXPANDER_E1, 5};
static constexpr NessoExpanderPin NESSO_LCD_BACKLIGHT = {NESSO_EXPANDER_E1, 6};
static constexpr NessoExpanderPin NESSO_LED_BUILTIN = {NESSO_EXPANDER_E1, 7};

class NessoExpander {
  bool initialized[2] = {false, false};

  int indexFor(uint8_t address) const;
  bool ensure(uint8_t address);
  bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);
  bool readRegister(uint8_t address, uint8_t reg, uint8_t& value);
  bool writeBitRegister(uint8_t address, uint8_t reg, uint8_t bit, bool value);
  bool readBitRegister(uint8_t address, uint8_t reg, uint8_t bit, bool& value);

public:
  bool begin();
	  bool pinMode(NessoExpanderPin pin, uint8_t mode);
	  bool digitalWrite(NessoExpanderPin pin, uint8_t value);
	  int digitalRead(NessoExpanderPin pin);
	  bool isInitialized(uint8_t address) const;
	#ifdef NESSO_DIAG
	  bool debugReadRegister(uint8_t address, uint8_t reg, uint8_t& value);
	#endif
};

class NessoExpanderButton {
  NessoExpanderPin pin;
  int8_t prev;
  bool reverse;
  int longMillis;
  unsigned long downAt;
  uint8_t clickCount;
  unsigned long lastClickTime;
  bool pendingClick;
  bool cancel;

  bool isPressed(int level) const;

public:
  NessoExpanderButton(NessoExpanderPin pin, int longPressMillis = 0, bool reverse = true);
  void begin();
  int check(bool repeatClick = false);
  void cancelClick();
  bool isPressed() const;
};

extern NessoExpander nessoExpander;
