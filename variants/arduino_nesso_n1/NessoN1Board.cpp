#include "NessoN1Board.h"

#if defined(NESSO_ENABLE_SAFE_OTA) && !defined(DISABLE_WIFI_OTA)
  #include <WiFi.h>
  #include <ESPAsyncWebServer.h>
  #include <AsyncElegantOTA.h>
  #include <SPIFFS.h>
#endif

static constexpr uint8_t BQ27220_I2C_ADDR = 0x55;
static constexpr uint8_t BQ27220_VOLTAGE_REG = 0x08;

#ifndef NESSO_BOOT_DIAG
  #define NESSO_BOOT_DIAG 0
#endif

#if NESSO_BOOT_DIAG && ARDUINO
  #define NESSO_BOARD_LOG(F, ...) do { Serial.printf("[NESSO_BOOT] " F "\n", ##__VA_ARGS__); Serial.flush(); } while (0)
#else
  #define NESSO_BOARD_LOG(...) {}
#endif

static bool readI2C16LE(uint8_t address, uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(address, (uint8_t)2) != 2) return false;
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  value = ((uint16_t)hi << 8) | lo;
  return true;
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
  bootMillis = millis();
  ESP32Board::begin();
  Wire.setClock(100000);
  expanderReady = nessoExpander.begin();
  const bool e0Ready = nessoExpander.isInitialized(NESSO_EXPANDER_E0);
  const bool e1Ready = nessoExpander.isInitialized(NESSO_EXPANDER_E1);
  if (!expanderReady) {
    NESSO_BOARD_LOG("expander warning e0=%s e1=%s", e0Ready ? "ok" : "fail", e1Ready ? "ok" : "fail");
  }

  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);
  pinMode(P_LORA_BUSY, INPUT);
  pinMode(P_LORA_DIO_1, INPUT);

#ifdef NESSO_DIAG
  Serial.printf("[NESSO_DIAG] expander begin: %s\n", expanderReady ? "ok" : "failed");
  dumpExpander(NESSO_EXPANDER_E0, "after begin E0");
  dumpExpander(NESSO_EXPANDER_E1, "after begin E1");
  dumpLoRaPins("after gpio init");
#endif

  if (e0Ready) {
    nessoExpander.pinMode(NESSO_KEY1, INPUT_PULLUP);
    nessoExpander.pinMode(NESSO_KEY2, INPUT_PULLUP);

    nessoExpander.digitalWrite(NESSO_LORA_ENABLE, LOW);
    nessoExpander.pinMode(NESSO_LORA_ENABLE, OUTPUT);
    delay(100);
    nessoExpander.digitalWrite(NESSO_LORA_ENABLE, HIGH);
    delay(100);

    nessoExpander.digitalWrite(NESSO_LORA_LNA_ENABLE, HIGH);
    nessoExpander.pinMode(NESSO_LORA_LNA_ENABLE, OUTPUT);
    nessoExpander.digitalWrite(NESSO_LORA_ANTENNA_SWITCH, HIGH);
    nessoExpander.pinMode(NESSO_LORA_ANTENNA_SWITCH, OUTPUT);
  }

  if (e1Ready) {
    nessoExpander.pinMode(NESSO_VIN_DETECT, INPUT);

    nessoExpander.digitalWrite(NESSO_LED_BUILTIN, LOW);
    nessoExpander.pinMode(NESSO_LED_BUILTIN, OUTPUT);

    nessoExpander.digitalWrite(NESSO_POWEROFF, LOW);
    nessoExpander.pinMode(NESSO_POWEROFF, OUTPUT);
  }

  // The fuel gauge can NACK during the first few hundred milliseconds of boot.
  // Defer battery reads so display startup does not produce I2C bus warnings.
  bootBatteryMilliVolts = 0;
  lastBatteryMilliVolts = 0;
  lastBatteryReadMillis = bootMillis;

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

bool NessoN1Board::isOtaButtonHeld() {
  return isKey1Held();
}

bool NessoN1Board::isKey1Held() {
  if (!nessoExpander.isInitialized(NESSO_EXPANDER_E0)) return false;
  return nessoExpander.digitalRead(NESSO_KEY1) == LOW;
}

bool NessoN1Board::isKey2Held() {
  if (!nessoExpander.isInitialized(NESSO_EXPANDER_E0)) return false;
  return nessoExpander.digitalRead(NESSO_KEY2) == LOW;
}

bool NessoN1Board::startOTAUpdate(const char* id, char reply[]) {
#if defined(NESSO_ENABLE_SAFE_OTA) && !defined(DISABLE_WIFI_OTA)
  if (!isOtaButtonHeld()) {
    strcpy(reply, "Hold KEY1 while running start ota");
    return false;
  }

  inhibit_sleep = true;
  SPIFFS.begin(true);

  uint8_t mac[6];
  WiFi.macAddress(mac);

  static char ssid[32];
  static char pass[16];
  snprintf(ssid, sizeof(ssid), "NessoN1-OTA-%02X%02X", mac[4], mac[5]);
  snprintf(pass, sizeof(pass), "N1%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, pass);

  static char idBuf[80];
  static char homeBuf[180];
  snprintf(idBuf, sizeof(idBuf), "%s (%s)", id, getManufacturerName());
  snprintf(homeBuf, sizeof(homeBuf),
           "<h2>Nesso N1 MeshCore OTA</h2><p>ID: %s</p><p>Use the /update endpoint.</p>",
           id);

  static AsyncWebServer* server = nullptr;
  if (server == nullptr) {
    server = new AsyncWebServer(80);
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/html", homeBuf);
    });
    server->on("/doctor", HTTP_GET, [this](AsyncWebServerRequest *request) {
      char diag[240];
      formatBoardDiagnostics(diag, sizeof(diag));
      request->send(200, "text/plain", diag);
    });
    AsyncElegantOTA.setID(idBuf);
    AsyncElegantOTA.begin(server);
    server->begin();
  }

  snprintf(reply, 160, "Started: ssid=%s pass=%s http://%s/update",
           ssid, pass, WiFi.softAPIP().toString().c_str());
  return true;
#else
  return ESP32Board::startOTAUpdate(id, reply);
#endif
}

bool NessoN1Board::formatBoardDiagnostics(char* reply, size_t max_len) {
  if (reply == nullptr || max_len == 0) return false;

  const bool e0 = nessoExpander.isInitialized(NESSO_EXPANDER_E0);
  const bool e1 = nessoExpander.isInitialized(NESSO_EXPANDER_E1);
  const uint16_t batt = getBattMilliVolts();
  const bool external = e1 && isExternalPowered();
  const int busy = digitalRead(P_LORA_BUSY);
  const int dio1 = digitalRead(P_LORA_DIO_1);
  const int nss = digitalRead(P_LORA_NSS);
  const int key1 = e0 ? nessoExpander.digitalRead(NESSO_KEY1) : LOW;
  const int key2 = e0 ? nessoExpander.digitalRead(NESSO_KEY2) : LOW;
  const int loraEn = e0 ? nessoExpander.digitalRead(NESSO_LORA_ENABLE) : LOW;
  const int lnaEn = e0 ? nessoExpander.digitalRead(NESSO_LORA_LNA_ENABLE) : LOW;
  const unsigned long uptimeSecs = millis() / 1000;

  snprintf(reply, max_len,
           "Nesso Doctor: expander=%s e0=%s e1=%s batt=%umV boot=%umV pwr=%s lora_en=%d lna=%d nss=%d busy=%d dio1=%d key1=%s key2=%s up=%lus",
           expanderReady ? "ok" : "fail",
           e0 ? "ok" : "fail",
           e1 ? "ok" : "fail",
           batt,
           bootBatteryMilliVolts,
           external ? "external" : "battery",
           loraEn,
           lnaEn,
           nss,
           busy,
           dio1,
           key1 == LOW ? "down" : "up",
           key2 == LOW ? "down" : "up",
           uptimeSecs);
  return expanderReady && e0 && e1 && batt > 0 && loraEn == HIGH;
}

bool NessoN1Board::isExternalPowered() {
  if (!nessoExpander.isInitialized(NESSO_EXPANDER_E1)) return false;
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
  if (lastBatteryMilliVolts == 0 && millis() - bootMillis < 2500) {
    return 0;
  }

  if (lastBatteryMilliVolts > 0 && millis() - lastBatteryReadMillis < 8000) {
    return lastBatteryMilliVolts;
  }

  uint16_t value = readBatteryMilliVolts();
  lastBatteryReadMillis = millis();
  if (value > 0) {
    lastBatteryMilliVolts = value;
    if (bootBatteryMilliVolts == 0) {
      bootBatteryMilliVolts = value;
    }
  }
  return lastBatteryMilliVolts;
}

const char* NessoN1Board::getManufacturerName() const {
  return "Arduino Nesso N1";
}
