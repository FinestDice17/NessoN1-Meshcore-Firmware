#include "NessoExpander.h"

#define NESSO_EXP_REG_RESET          0x01
#define NESSO_EXP_REG_OUTPUT_ENABLE  0x03
#define NESSO_EXP_REG_OUTPUT         0x05
#define NESSO_EXP_REG_HIGH_Z         0x07
#define NESSO_EXP_REG_INPUT_DEFAULT  0x09
#define NESSO_EXP_REG_PULL_ENABLE    0x0B
#define NESSO_EXP_REG_PULL_SELECT    0x0D
#define NESSO_EXP_REG_INPUT          0x0F
#define NESSO_EXP_REG_INT_MASK       0x11
#define NESSO_EXP_REG_INT_STATUS     0x13

#define NESSO_MULTI_CLICK_WINDOW_MS  280

NessoExpander nessoExpander;

int NessoExpander::indexFor(uint8_t address) const {
  if (address == NESSO_EXPANDER_E0) return 0;
  if (address == NESSO_EXPANDER_E1) return 1;
  return -1;
}

bool NessoExpander::writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool NessoExpander::readRegister(uint8_t address, uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(address, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

bool NessoExpander::writeBitRegister(uint8_t address, uint8_t reg, uint8_t bit, bool value) {
  uint8_t regValue = 0;
  if (!readRegister(address, reg, regValue)) return false;
  if (value) {
    regValue |= (1 << bit);
  } else {
    regValue &= ~(1 << bit);
  }
  return writeRegister(address, reg, regValue);
}

bool NessoExpander::readBitRegister(uint8_t address, uint8_t reg, uint8_t bit, bool& value) {
  uint8_t regValue = 0;
  if (!readRegister(address, reg, regValue)) return false;
  value = (regValue & (1 << bit)) != 0;
  return true;
}

bool NessoExpander::ensure(uint8_t address) {
  int idx = indexFor(address);
  if (idx < 0) return false;
  if (initialized[idx]) return true;

  uint8_t ignored = 0;
  readRegister(address, NESSO_EXP_REG_RESET, ignored);
  if (!writeRegister(address, NESSO_EXP_REG_RESET, 0x01)) return false;
  if (!readRegister(address, NESSO_EXP_REG_RESET, ignored)) return false;

  if (!writeRegister(address, NESSO_EXP_REG_INPUT_DEFAULT, 0xFF)) return false;
  if (!writeRegister(address, NESSO_EXP_REG_INT_MASK, 0xFF)) return false;
  if (!writeRegister(address, NESSO_EXP_REG_PULL_ENABLE, 0x00)) return false;
  if (!writeRegister(address, NESSO_EXP_REG_OUTPUT_ENABLE, 0x00)) return false;
  if (!readRegister(address, NESSO_EXP_REG_INT_STATUS, ignored)) return false;

  initialized[idx] = true;
  return true;
}

bool NessoExpander::begin() {
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    bool e0 = ensure(NESSO_EXPANDER_E0);
    bool e1 = ensure(NESSO_EXPANDER_E1);
    if (e0 && e1) return true;
    delay(20);
  }
  return isInitialized(NESSO_EXPANDER_E0) && isInitialized(NESSO_EXPANDER_E1);
}

bool NessoExpander::isInitialized(uint8_t address) const {
  int idx = indexFor(address);
  return idx >= 0 && initialized[idx];
}

	#ifdef NESSO_DIAG
bool NessoExpander::debugReadRegister(uint8_t address, uint8_t reg, uint8_t& value) {
  return readRegister(address, reg, value);
}
#endif

bool NessoExpander::pinMode(NessoExpanderPin pin, uint8_t mode) {
  if (!ensure(pin.address)) return false;

  if (!writeBitRegister(pin.address, NESSO_EXP_REG_OUTPUT_ENABLE, pin.pin, mode == OUTPUT)) return false;
  if (mode == OUTPUT) {
    return writeBitRegister(pin.address, NESSO_EXP_REG_PULL_ENABLE, pin.pin, false) &&
           writeBitRegister(pin.address, NESSO_EXP_REG_HIGH_Z, pin.pin, false);
  }
  if (mode == INPUT_PULLUP) {
    return writeBitRegister(pin.address, NESSO_EXP_REG_PULL_ENABLE, pin.pin, true) &&
           writeBitRegister(pin.address, NESSO_EXP_REG_PULL_SELECT, pin.pin, true);
  }
  if (mode == INPUT_PULLDOWN) {
    return writeBitRegister(pin.address, NESSO_EXP_REG_PULL_ENABLE, pin.pin, true) &&
           writeBitRegister(pin.address, NESSO_EXP_REG_PULL_SELECT, pin.pin, false);
  }
  return writeBitRegister(pin.address, NESSO_EXP_REG_PULL_ENABLE, pin.pin, false);
}

bool NessoExpander::digitalWrite(NessoExpanderPin pin, uint8_t value) {
  if (!ensure(pin.address)) return false;
  return writeBitRegister(pin.address, NESSO_EXP_REG_OUTPUT, pin.pin, value == HIGH);
}

int NessoExpander::digitalRead(NessoExpanderPin pin) {
  if (!ensure(pin.address)) return LOW;
  bool value = false;
  if (!readBitRegister(pin.address, NESSO_EXP_REG_INPUT, pin.pin, value)) return LOW;
  return value ? HIGH : LOW;
}

NessoExpanderButton::NessoExpanderButton(NessoExpanderPin pin, int longPressMillis, bool reverse)
    : pin(pin), prev(reverse ? HIGH : LOW), reverse(reverse), longMillis(longPressMillis),
      downAt(0), clickCount(0), lastClickTime(0), pendingClick(false), cancel(false) {
}

void NessoExpanderButton::begin() {
  nessoExpander.pinMode(pin, reverse ? INPUT_PULLUP : INPUT_PULLDOWN);
  prev = nessoExpander.digitalRead(pin);
}

bool NessoExpanderButton::isPressed(int level) const {
  return reverse ? level == LOW : level != LOW;
}

bool NessoExpanderButton::isPressed() const {
  return isPressed(nessoExpander.digitalRead(pin));
}

void NessoExpanderButton::cancelClick() {
  cancel = true;
  downAt = 0;
  clickCount = 0;
  lastClickTime = 0;
  pendingClick = false;
}

int NessoExpanderButton::check(bool repeatClick) {
  int event = BUTTON_EVENT_NONE;
  int level = nessoExpander.digitalRead(pin);

  if (level != prev) {
    if (isPressed(level)) {
      downAt = millis();
    } else {
      if (longMillis > 0) {
        if (downAt > 0 && (unsigned long)(millis() - downAt) < (unsigned long)longMillis) {
          clickCount++;
          lastClickTime = millis();
          pendingClick = true;
        }
      } else {
        clickCount++;
        lastClickTime = millis();
        pendingClick = true;
      }
      downAt = 0;
    }
    prev = level;
  }

  if (!isPressed(level) && cancel) {
    cancel = false;
  }

  if (longMillis > 0 && downAt > 0 && (unsigned long)(millis() - downAt) >= (unsigned long)longMillis) {
    if (pendingClick) {
      cancelClick();
    } else {
      event = BUTTON_EVENT_LONG_PRESS;
      downAt = 0;
      clickCount = 0;
      lastClickTime = 0;
      pendingClick = false;
    }
  }

  if (downAt > 0 && repeatClick && (unsigned long)(millis() - downAt) >= 700) {
    event = BUTTON_EVENT_CLICK;
  }

  if (pendingClick && (unsigned long)(millis() - lastClickTime) >= NESSO_MULTI_CLICK_WINDOW_MS) {
    if (downAt > 0) return event;
    if (!cancel) {
      if (clickCount == 1) {
        event = BUTTON_EVENT_CLICK;
      } else if (clickCount == 2) {
        event = BUTTON_EVENT_DOUBLE_CLICK;
      } else if (clickCount >= 3) {
        event = BUTTON_EVENT_TRIPLE_CLICK;
      }
    }
    cancel = false;
    clickCount = 0;
    lastClickTime = 0;
    pendingClick = false;
  }

  return event;
}
