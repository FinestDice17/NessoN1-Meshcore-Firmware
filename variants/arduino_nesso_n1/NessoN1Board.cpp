#include "NessoN1Board.h"

static constexpr uint8_t BQ27220_I2C_ADDR = 0x55;
static constexpr uint8_t BQ27220_VOLTAGE_REG = 0x08;

static constexpr uint8_t AW32001_I2C_ADDR = 0x49;
static constexpr uint8_t AW32001_REG_CTRL0 = 0x01;   // CEB (charge-enable) bit
static constexpr uint8_t AW32001_REG_TIMER = 0x05;   // WATCHDOG bits
static constexpr uint8_t AW32001_CEB_BIT = 3;
static constexpr uint8_t AW32001_WATCHDOG_SHIFT = 5;
static constexpr uint8_t AW32001_WATCHDOG_MASK = 0x3 << AW32001_WATCHDOG_SHIFT;

static bool readI2C16LE(uint8_t address, uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, (uint8_t)2) != 2) return false;
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  value = ((uint16_t)hi << 8) | lo;
  return true;
}

static bool readAw32001Reg(uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(AW32001_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(AW32001_I2C_ADDR, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

static void writeAw32001Reg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(AW32001_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// The AW32001E charger's CEB (charge-enable, active-low) bit resets to 1
// (charging disabled, VBUS power-path only) on every power-up and is never
// set by the ROM/bootloader. Without this, the board runs fine on USB but
// never actually charges the battery, no matter how long it's plugged in.
//
// Any I2C write also switches the chip into "Host Mode", which arms its
// watchdog timer (160s by default). If the host doesn't periodically pet it
// via REG02H[6], the watchdog expiring turns BOTH the battery and system
// FETs off momentarily -- a real, hard power-cycle of the whole board, not
// just a firmware crash. Since this firmware has no periodic charger
// upkeep, disable the watchdog outright (REG05H[6:5]=00) instead.
static void enableCharging() {
  uint8_t reg01;
  if (readAw32001Reg(AW32001_REG_CTRL0, reg01)) {
    reg01 &= ~(1 << AW32001_CEB_BIT);
    writeAw32001Reg(AW32001_REG_CTRL0, reg01);
  }

  uint8_t reg05;
  if (readAw32001Reg(AW32001_REG_TIMER, reg05)) {
    reg05 &= ~AW32001_WATCHDOG_MASK;
    writeAw32001Reg(AW32001_REG_TIMER, reg05);
  }
}

#ifdef NESSO_DIAG
static void dumpExpander(uint8_t address, const char* label) {
  Serial.printf("[NESSO_DIAG] %s 0x%02X", label, address);
  for (uint8_t reg = 0x01; reg <= 0x13; reg += 2) {
    uint8_t value = 0;
    if (nessoExpander.debugReadRegister(address, reg, value)) {
      Serial.printf(" r%02X=%02X", reg, value);
    } else {
      Serial.printf(" r%02X=??", reg);
    }
  }
  Serial.println();
}

static void dumpLoRaPins(const char* label) {
  Serial.printf("[NESSO_DIAG] %s NSS=%d BUSY=%d DIO1=%d\n",
                label,
                digitalRead(P_LORA_NSS),
                digitalRead(P_LORA_BUSY),
                digitalRead(P_LORA_DIO_1));
}
#endif

void NessoN1Board::begin() {
  ESP32Board::begin();
  Wire.setClock(400000);
  bool expanderOk = nessoExpander.begin();
  enableCharging();

  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);
  pinMode(P_LORA_BUSY, INPUT);
  pinMode(P_LORA_DIO_1, INPUT);

#ifdef NESSO_DIAG
  Serial.printf("[NESSO_DIAG] expander begin: %s\n", expanderOk ? "ok" : "failed");
  dumpExpander(NESSO_EXPANDER_E0, "after begin E0");
  dumpExpander(NESSO_EXPANDER_E1, "after begin E1");
  dumpLoRaPins("after gpio init");
#endif

  nessoExpander.pinMode(NESSO_KEY1, INPUT_PULLUP);
  nessoExpander.pinMode(NESSO_KEY2, INPUT_PULLUP);
  nessoExpander.pinMode(NESSO_VIN_DETECT, INPUT);

  nessoExpander.digitalWrite(NESSO_LED_BUILTIN, LOW);
  nessoExpander.pinMode(NESSO_LED_BUILTIN, OUTPUT);

  nessoExpander.digitalWrite(NESSO_POWEROFF, LOW);
  nessoExpander.pinMode(NESSO_POWEROFF, OUTPUT);

  nessoExpander.digitalWrite(NESSO_LORA_ENABLE, LOW);
  nessoExpander.pinMode(NESSO_LORA_ENABLE, OUTPUT);
  delay(100);
  nessoExpander.digitalWrite(NESSO_LORA_ENABLE, HIGH);
  delay(100);

  nessoExpander.digitalWrite(NESSO_LORA_LNA_ENABLE, HIGH);
  nessoExpander.pinMode(NESSO_LORA_LNA_ENABLE, OUTPUT);
  nessoExpander.digitalWrite(NESSO_LORA_ANTENNA_SWITCH, HIGH);
  nessoExpander.pinMode(NESSO_LORA_ANTENNA_SWITCH, OUTPUT);

#ifdef NESSO_DIAG
  dumpExpander(NESSO_EXPANDER_E0, "after lora enable E0");
  dumpLoRaPins("after lora enable");
#endif
}

void NessoN1Board::onBeforeTransmit() {
  nessoExpander.digitalWrite(NESSO_LED_BUILTIN, HIGH);
  nessoExpander.digitalWrite(NESSO_LORA_LNA_ENABLE, LOW);
}

void NessoN1Board::onAfterTransmit() {
  nessoExpander.digitalWrite(NESSO_LORA_LNA_ENABLE, HIGH);
  nessoExpander.digitalWrite(NESSO_LED_BUILTIN, LOW);
}

void NessoN1Board::powerOff() {
  nessoExpander.digitalWrite(NESSO_LED_BUILTIN, LOW);
  nessoExpander.digitalWrite(NESSO_LCD_BACKLIGHT, LOW);
  nessoExpander.digitalWrite(NESSO_LORA_LNA_ENABLE, LOW);
  nessoExpander.digitalWrite(NESSO_LORA_ANTENNA_SWITCH, LOW);
  nessoExpander.digitalWrite(NESSO_LORA_ENABLE, LOW);

  nessoExpander.digitalWrite(NESSO_POWEROFF, HIGH);
  delay(250);
  nessoExpander.digitalWrite(NESSO_POWEROFF, LOW);

  while (true) {
    delay(1000);
  }
}

bool NessoN1Board::isExternalPowered() {
  return nessoExpander.digitalRead(NESSO_VIN_DETECT) == HIGH;
}

uint16_t NessoN1Board::readBatteryMilliVolts() {
  uint16_t value = 0;
  if (!readI2C16LE(BQ27220_I2C_ADDR, BQ27220_VOLTAGE_REG, value)) {
    return 0;
  }
  return value;
}

uint16_t NessoN1Board::getBattMilliVolts() {
  uint16_t value = readBatteryMilliVolts();
  if (value > 0) {
    lastBatteryMilliVolts = value;
  }
  return lastBatteryMilliVolts;
}

const char* NessoN1Board::getManufacturerName() const {
  return "Arduino Nesso N1";
}
