#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>
#include "MyMesh.h"

// Believe it or not, this std C function is busted on some platforms!
static uint32_t _atoi(const char* sp) {
  uint32_t n = 0;
  while (*sp && *sp >= '0' && *sp <= '9') {
    n *= 10;
    n += (*sp++ - '0');
  }
  return n;
}

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #include <InternalFileSystem.h>
  #if defined(QSPIFLASH)
    #include <CustomLFS_QSPIFlash.h>
    DataStore store(InternalFS, QSPIFlash, rtc_clock);
  #else
  #if defined(EXTRAFS)
    #include <CustomLFS.h>
    CustomLFS ExtraFS(0xD4000, 0x19000, 128);
    DataStore store(InternalFS, ExtraFS, rtc_clock);
  #else
    DataStore store(InternalFS, rtc_clock);
  #endif
  #endif
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
  DataStore store(LittleFS, rtc_clock);
#elif defined(ESP32)
  #include <SPIFFS.h>
  DataStore store(SPIFFS, rtc_clock);
#endif

#ifdef ESP32
  #ifdef NESSO_SMART_COMPANION
    #include <WiFi.h>
    #include <helpers/esp32/SerialMultiInterface.h>
    SerialMultiInterface serial_interface;
    #ifndef TCP_PORT
      #define TCP_PORT 5000
    #endif
  #elif defined(WIFI_SSID)
    #include <helpers/esp32/SerialWifiInterface.h>
    SerialWifiInterface serial_interface;
    #ifndef TCP_PORT
      #define TCP_PORT 5000
    #endif
  #elif defined(BLE_PIN_CODE)
    #include <helpers/esp32/SerialBLEInterface.h>
    SerialBLEInterface serial_interface;
  #elif defined(SERIAL_RX)
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
    HardwareSerial companion_serial(1);
  #else
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
  #endif
#elif defined(RP2040_PLATFORM)
  //#ifdef WIFI_SSID
  //  #include <helpers/rp2040/SerialWifiInterface.h>
  //  SerialWifiInterface serial_interface;
  //  #ifndef TCP_PORT
  //    #define TCP_PORT 5000
  //  #endif
  // #elif defined(BLE_PIN_CODE)
  //   #include <helpers/rp2040/SerialBLEInterface.h>
  //   SerialBLEInterface serial_interface;
  #if defined(SERIAL_RX)
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
    HardwareSerial companion_serial(1);
  #else
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
  #endif
#elif defined(NRF52_PLATFORM)
  #ifdef BLE_PIN_CODE
    #include <helpers/nrf52/SerialBLEInterface.h>
    SerialBLEInterface serial_interface;
  #else
    #include <helpers/ArduinoSerialInterface.h>
    ArduinoSerialInterface serial_interface;
  #endif
#elif defined(STM32_PLATFORM)
  #include <helpers/ArduinoSerialInterface.h>
  ArduinoSerialInterface serial_interface;
#else
  #error "need to define a serial interface"
#endif

/* GLOBAL OBJECTS */
#ifdef DISPLAY_CLASS
  #include "UITask.h"
  UITask ui_task(&board, &serial_interface);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;
MyMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store
   #ifdef DISPLAY_CLASS
      , &ui_task
   #endif
);

/* END GLOBAL OBJECTS */

#if defined(DISPLAY_CLASS) && defined(FRENET_TDECK_BRIDGE_STATUS_SCREEN)
static void frenetRenderBridgeStatus(DisplayDriver* disp) {
  if (disp == NULL) return;
  if (!disp->isOn()) {
    disp->turnOn();
  }

  NodePrefs* prefs = the_mesh.getNodePrefs();
  char line[48];

  disp->startFrame();
  disp->setTextSize(1);
  disp->setColor(DisplayDriver::YELLOW);
  disp->drawTextCentered(disp->width() / 2, 0, "FreNET USB Bridge");

  disp->setColor(DisplayDriver::GREEN);
  disp->drawTextCentered(disp->width() / 2, 12, "READY");

  disp->setColor(DisplayDriver::LIGHT);
  snprintf(line, sizeof(line), "Node %s", prefs->node_name);
  disp->drawTextCentered(disp->width() / 2, 24, line);
  snprintf(line, sizeof(line), "%06.3f MHz SF%d", prefs->freq, prefs->sf);
  disp->drawTextCentered(disp->width() / 2, 36, line);
  snprintf(line, sizeof(line), "BW %03.2f CR%d TX%d", prefs->bw, prefs->cr, prefs->tx_power_dbm);
  disp->drawTextCentered(disp->width() / 2, 48, line);

  disp->setColor(DisplayDriver::ORANGE);
  disp->drawTextCentered(disp->width() / 2, 58, "USB only / WiFi off");
  disp->endFrame();
}
#endif

#ifndef NESSO_BOOT_DIAG
  #define NESSO_BOOT_DIAG 0
#endif

#if NESSO_BOOT_DIAG
  #define NESSO_BOOT_LOG(F, ...) do { Serial.printf("[NESSO_BOOT] " F "\n", ##__VA_ARGS__); Serial.flush(); } while (0)
#else
  #define NESSO_BOOT_LOG(...) {}
#endif

#if defined(ESP32) && defined(NESSO_SMART_COMPANION)
enum NessoCompanionMode : uint8_t {
  NESSO_COMPANION_BLE = 0,
  NESSO_COMPANION_WIFI = 1,
};

static constexpr const char* NESSO_MODE_FILE = "/nesso_mode";
static NessoCompanionMode nesso_companion_mode = NESSO_COMPANION_BLE;
static bool nesso_companion_mode_forced = false;
static unsigned long nesso_mode_reboot_at = 0;

static const char* nessoCompanionModeName(NessoCompanionMode mode) {
  return mode == NESSO_COMPANION_WIFI ? "wifi" : "ble";
}

const char* nessoGetCompanionModeName() {
  return nessoCompanionModeName(nesso_companion_mode);
}

uint8_t nessoGetCompanionModeCode() {
  return nesso_companion_mode == NESSO_COMPANION_WIFI ? 1 : 0;
}

static bool nessoParseCompanionMode(const char* value, NessoCompanionMode& mode) {
  if (value == nullptr) return false;
  while (*value == ' ') value++;

  if (strcmp(value, "ble") == 0 || strcmp(value, "bluetooth") == 0 || strcmp(value, "bt") == 0) {
    mode = NESSO_COMPANION_BLE;
    return true;
  }
  if (strcmp(value, "wifi") == 0 || strcmp(value, "wi-fi") == 0 || strcmp(value, "tcp") == 0) {
    mode = NESSO_COMPANION_WIFI;
    return true;
  }
  return false;
}

static void nessoLoadCompanionMode() {
  nesso_companion_mode = NESSO_COMPANION_BLE;
  File file = SPIFFS.open(NESSO_MODE_FILE);
  if (!file) return;

  char mode_buf[16] = {0};
  size_t len = file.readBytes(mode_buf, sizeof(mode_buf) - 1);
  file.close();
  while (len > 0 && (mode_buf[len - 1] == '\n' || mode_buf[len - 1] == '\r' || mode_buf[len - 1] == ' ')) {
    mode_buf[--len] = 0;
  }

  NessoCompanionMode parsed;
  if (nessoParseCompanionMode(mode_buf, parsed)) {
    nesso_companion_mode = parsed;
  }
}

static bool nessoSaveCompanionMode(NessoCompanionMode mode) {
  File file = SPIFFS.open(NESSO_MODE_FILE, "w");
  if (!file) return false;
  file.print(nessoCompanionModeName(mode));
  file.print('\n');
  file.close();
  return true;
}

static void nessoScheduleModeReboot();

bool nessoSetCompanionModeCode(uint8_t mode_code) {
  NessoCompanionMode requested;
  if (mode_code == 0) {
    requested = NESSO_COMPANION_BLE;
  } else if (mode_code == 1) {
    requested = NESSO_COMPANION_WIFI;
  } else {
    return false;
  }

  if (!nessoSaveCompanionMode(requested)) {
    return false;
  }

  nesso_companion_mode = requested;
  nesso_companion_mode_forced = false;
  nessoScheduleModeReboot();
  return true;
}

static void nessoScheduleModeReboot() {
  nesso_mode_reboot_at = millis() + 900;
}

bool nessoHandleCompanionModeCommand(const char* command, char* reply, size_t reply_len) {
  if (command == nullptr || reply == nullptr || reply_len == 0) return false;

  const char* mode_arg = nullptr;
  if (strcmp(command, "mode") == 0 || strcmp(command, "mode status") == 0 ||
      strcmp(command, "companion mode") == 0 || strcmp(command, "get companion.mode") == 0) {
    snprintf(reply, reply_len, "companion mode: %s%s", nessoGetCompanionModeName(), nesso_companion_mode_forced ? " (button-forced)" : "");
    return true;
  } else if (strncmp(command, "mode ", 5) == 0) {
    mode_arg = command + 5;
  } else if (strncmp(command, "companion mode ", 15) == 0) {
    mode_arg = command + 15;
  } else if (strcmp(command, "start wifi") == 0 || strcmp(command, "wifi start") == 0) {
    mode_arg = "wifi";
  } else if (strcmp(command, "start ble") == 0 || strcmp(command, "ble start") == 0 ||
             strcmp(command, "start bluetooth") == 0 || strcmp(command, "bluetooth start") == 0) {
    mode_arg = "ble";
  } else {
    return false;
  }

  NessoCompanionMode requested;
  if (!nessoParseCompanionMode(mode_arg, requested)) {
    snprintf(reply, reply_len, "Error: mode ble|wifi");
    return true;
  }

  if (!nessoSaveCompanionMode(requested)) {
    snprintf(reply, reply_len, "Error: could not save mode");
    return true;
  }

  nesso_companion_mode = requested;
  nesso_companion_mode_forced = false;
  nessoScheduleModeReboot();
  snprintf(reply, reply_len, "OK - companion mode %s saved; rebooting", nessoCompanionModeName(requested));
  return true;
}

static void nessoApplyBootModeOverride() {
#if defined(NESSO_N1_BOARD)
  if (board.isKey1Held()) {
    nesso_companion_mode = NESSO_COMPANION_WIFI;
    nesso_companion_mode_forced = true;
    NESSO_BOOT_LOG("KEY1 boot override: wifi mode");
  } else if (board.isKey2Held()) {
    nesso_companion_mode = NESSO_COMPANION_BLE;
    nesso_companion_mode_forced = true;
    NESSO_BOOT_LOG("KEY2 boot override: ble mode");
  }
#endif
}

static void nessoMaybeRebootForModeChange() {
  if (nesso_mode_reboot_at != 0 && millis() - nesso_mode_reboot_at < 0x80000000UL) {
    Serial.flush();
    delay(50);
    board.reboot();
  }
}
#endif

void halt() {
  while (1) {
    NESSO_BOOT_LOG("halted");
    delay(1000);
  }
}

/* WIFI RECONNECT TRACKERS */
#if defined(ESP32) && defined(WIFI_SSID) && !defined(WIFI_AP_MODE)
  bool wifi_needs_reconnect = false;
  unsigned long last_wifi_reconnect_attempt = 0;
#endif

#if defined(ESP32) && defined(NESSO_SMART_COMPANION)
  #ifndef NESSO_CONNECTIVITY_LOGGING
    #define NESSO_CONNECTIVITY_LOGGING 1
  #endif
#endif

#if defined(ESP32) && defined(WIFI_SSID)
static bool wifi_companion_started = false;

static bool setupWiFiCompanion() {
  if (wifi_companion_started) return true;

#ifdef WIFI_AP_MODE
  WiFi.mode(WIFI_AP);
  bool started = false;
#ifdef WIFI_PWD
  started = WiFi.softAP(WIFI_SSID, WIFI_PWD);
#else
  started = WiFi.softAP(WIFI_SSID);
#endif
  if (started) {
    wifi_companion_started = true;
    WIFI_DEBUG_PRINTLN("WiFi AP started: %s", WIFI_SSID);
#if defined(NESSO_CONNECTIVITY_LOGGING) && NESSO_CONNECTIVITY_LOGGING
    Serial.printf("Nesso: WiFi AP started: %s, TCP %d\n", WIFI_SSID, TCP_PORT);
#endif
  } else {
    WIFI_DEBUG_PRINTLN("WiFi AP start failed: %s", WIFI_SSID);
#if defined(NESSO_CONNECTIVITY_LOGGING) && NESSO_CONNECTIVITY_LOGGING
    Serial.printf("Nesso: WiFi AP start failed: %s\n", WIFI_SSID);
#endif
  }
  return started;
#else
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
          WIFI_DEBUG_PRINTLN("WiFi disconnected. Flagging for reconnect...");
          wifi_needs_reconnect = true;
      } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
          WIFI_DEBUG_PRINTLN("WiFi connected successfully!");
          wifi_needs_reconnect = false;
      }
  });

  WiFi.begin(WIFI_SSID, WIFI_PWD);
  wifi_companion_started = true;
  return true;
#endif
}
#endif

#if defined(ESP32) && defined(NESSO_SMART_COMPANION) && defined(WIFI_SSID)
  #ifndef NESSO_WIFI_AUTO_START
    #define NESSO_WIFI_AUTO_START 1
  #endif
  #ifndef NESSO_WIFI_START_DELAY_MS
    #define NESSO_WIFI_START_DELAY_MS 30000
  #endif
  #ifndef NESSO_WIFI_START_AFTER_BLE_MS
    #define NESSO_WIFI_START_AFTER_BLE_MS 3000
  #endif

static unsigned long nesso_wifi_earliest_start = 0;
static unsigned long nesso_ble_connected_at = 0;

static void maybeStartSmartWiFiCompanion() {
  if (!NESSO_WIFI_AUTO_START) return;
  if (nesso_companion_mode != NESSO_COMPANION_BLE) return;
  if (wifi_companion_started) return;

  unsigned long now = millis();
  if (serial_interface.isBleConnected()) {
    if (nesso_ble_connected_at == 0) {
      nesso_ble_connected_at = now;
    }
    if (now - nesso_ble_connected_at >= NESSO_WIFI_START_AFTER_BLE_MS && setupWiFiCompanion()) {
      serial_interface.beginWiFi();
    }
    return;
  }

  nesso_ble_connected_at = 0;
  if (nesso_wifi_earliest_start != 0 && now - nesso_wifi_earliest_start < 0x80000000UL) {
    if (setupWiFiCompanion()) {
      serial_interface.beginWiFi();
    }
  }
}
#endif

void setup() {
  // ---- ESP32-C6 / USB-Serial-JTAG (HWCDC) reliability tuning ----
  // The C6 (and C3/H2) boot their USB console on the USB-Serial-JTAG peripheral, i.e.
  // arduino-esp32's HWCDC. Its RX path is a small 256-byte single-byte FreeRTOS queue
  // filled from a 64-byte HW FIFO in an ISR that SILENTLY DROPS bytes once the queue is
  // full, and its write() blocks the (single-threaded) main loop for up to tx_timeout_ms
  // when the host is slow to drain. Under bidirectional companion-serial load the largest
  // inbound frame (CMD_SEND_CHANNEL_DATA, ~80 bytes) overflows RX; a dropped length byte
  // then desyncs the frame parser permanently (device goes silent until a power-cycle).
  // The ESP32-S3 companion build uses native USB-OTG CDC and is unaffected. Enlarge both
  // buffers and make writes non-blocking so the loop can always keep draining RX. These
  // must run BEFORE begin() (begin only installs its 256-byte defaults if none preset).
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32H2)
  Serial.setRxBufferSize(4096);
  Serial.setTxBufferSize(4096);
  Serial.setTxTimeoutMs(0);
#endif
  Serial.begin(115200);
  delay(100);
  NESSO_BOOT_LOG("setup start");

  NESSO_BOOT_LOG("board begin");
  board.begin();
  NESSO_BOOT_LOG("board ready");

#ifdef DISPLAY_CLASS
  DisplayDriver* disp = NULL;
  NESSO_BOOT_LOG("display begin");
  if (display.begin()) {
    disp = &display;
    disp->startFrame();
  #ifdef ST7789
    disp->setTextSize(2);
  #endif
    disp->drawTextCentered(disp->width() / 2, 28, "Loading...");
    disp->endFrame();
  }
  NESSO_BOOT_LOG("display %s", disp != NULL ? "ready" : "not-ready");
#endif

  NESSO_BOOT_LOG("radio init");
  if (!radio_init()) {
    NESSO_BOOT_LOG("radio init failed");
    halt();
  }
  NESSO_BOOT_LOG("radio ready");

  fast_rng.begin(radio_driver.getRngSeed());
  NESSO_BOOT_LOG("rng ready");

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  #if defined(QSPIFLASH)
    if (!QSPIFlash.begin()) {
      // debug output might not be available at this point, might be too early. maybe should fall back to InternalFS here?
      MESH_DEBUG_PRINTLN("CustomLFS_QSPIFlash: failed to initialize");
    } else {
      MESH_DEBUG_PRINTLN("CustomLFS_QSPIFlash: initialized successfully");
    }
  #else
  #if defined(EXTRAFS)
      ExtraFS.begin();
  #endif
  #endif
  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );

#ifdef BLE_PIN_CODE
  serial_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
#else
  serial_interface.begin(Serial);
#endif
  the_mesh.startInterface(serial_interface);
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  store.begin();
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );

  //#ifdef WIFI_SSID
  //  WiFi.begin(WIFI_SSID, WIFI_PWD);
  //  serial_interface.begin(TCP_PORT);
  // #elif defined(BLE_PIN_CODE)
  //   char dev_name[32+16];
  //   sprintf(dev_name, "%s%s", BLE_NAME_PREFIX, the_mesh.getNodeName());
  //   serial_interface.begin(dev_name, the_mesh.getBLEPin());
  #if defined(SERIAL_RX)
    companion_serial.setPins(SERIAL_RX, SERIAL_TX);
    companion_serial.begin(115200);
    serial_interface.begin(companion_serial);
  #else
    serial_interface.begin(Serial);
  #endif
    the_mesh.startInterface(serial_interface);
#elif defined(ESP32)
  NESSO_BOOT_LOG("spiffs begin");
  SPIFFS.begin(true);
  NESSO_BOOT_LOG("store begin");
  store.begin();
  NESSO_BOOT_LOG("mesh begin");
  the_mesh.begin(
    #ifdef DISPLAY_CLASS
        disp != NULL
    #else
        false
    #endif
  );
  NESSO_BOOT_LOG("mesh ready");

#ifdef NESSO_SMART_COMPANION
  board.setInhibitSleep(true);   // Companion transports should remain awake while charging or connected.
  nessoLoadCompanionMode();
  nessoApplyBootModeOverride();
  if (nesso_companion_mode == NESSO_COMPANION_BLE) {
    WiFi.mode(WIFI_OFF);
  }
  if (NESSO_WIFI_AUTO_START && nesso_companion_mode == NESSO_COMPANION_BLE) {
    nesso_wifi_earliest_start = millis() + NESSO_WIFI_START_DELAY_MS;
  }
  NESSO_BOOT_LOG("smart serial begin mode=%s forced=%d wifi_auto=%d tcp=%d",
                 nessoGetCompanionModeName(), nesso_companion_mode_forced, NESSO_WIFI_AUTO_START, TCP_PORT);
  serial_interface.begin(Serial, BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin(), TCP_PORT,
                         nesso_companion_mode == NESSO_COMPANION_BLE);
  if (nesso_companion_mode == NESSO_COMPANION_WIFI) {
    if (setupWiFiCompanion()) {
      serial_interface.beginWiFi();
    }
  }
#elif defined(WIFI_SSID)
  board.setInhibitSleep(true);   // prevent sleep when WiFi is active
  setupWiFiCompanion();
  serial_interface.begin(TCP_PORT);
#elif defined(BLE_PIN_CODE)
  serial_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
#elif defined(SERIAL_RX)
  companion_serial.setPins(SERIAL_RX, SERIAL_TX);
  companion_serial.begin(115200);
  serial_interface.begin(companion_serial);
#else
  serial_interface.begin(Serial);
#endif
  NESSO_BOOT_LOG("serial begin ready");
  the_mesh.startInterface(serial_interface);
  NESSO_BOOT_LOG("serial enabled");
#else
  #error "need to define filesystem"
#endif

  NESSO_BOOT_LOG("sensors begin");
#ifndef FRENET_TDECK_BRIDGE_NO_SENSORS
  sensors.begin();
#else
  NESSO_BOOT_LOG("sensors skipped for FreNET T-Deck bridge proof");
#endif
  NESSO_BOOT_LOG("setup complete");

#if ENV_INCLUDE_GPS == 1 && !defined(FRENET_TDECK_BRIDGE_NO_SENSORS)
  the_mesh.applyGpsPrefs();
#endif

#ifdef DISPLAY_CLASS
  ui_task.begin(disp, &sensors, the_mesh.getNodePrefs());  // still want to pass this in as dependency, as prefs might be moved
#if defined(FRENET_TDECK_BRIDGE_STATUS_SCREEN)
  frenetRenderBridgeStatus(disp);
#endif
#endif

  board.onBootComplete();
}

void loop() {
  the_mesh.loop();
#ifndef FRENET_TDECK_BRIDGE_NO_SENSORS
  sensors.loop();
#endif
#ifdef DISPLAY_CLASS
#if defined(FRENET_TDECK_BRIDGE_STATUS_SCREEN)
  // Bridge status is static and painted once in setup(); a periodic full-frame
  // pushSprite() re-blits identical content and visibly flickers the panel, so
  // there is intentionally no repaint in loop().
#else
  ui_task.loop();
#endif
#endif
#if defined(ESP32) && defined(NESSO_SMART_COMPANION) && defined(WIFI_SSID)
  maybeStartSmartWiFiCompanion();
#endif
  rtc_clock.tick();

  if (!the_mesh.hasPendingWork()) {
#if defined(NRF52_PLATFORM)
    board.sleep(0); // nrf ignores seconds param, sleeps whenever possible
#endif
#if defined(ESP32) && defined(NESSO_COMPANION_LIGHT_SLEEP)
    if (!serial_interface.isConnected()) {
#ifndef NESSO_COMPANION_LIGHT_SLEEP_SECS
      #define NESSO_COMPANION_LIGHT_SLEEP_SECS 3
#endif
      board.sleep(NESSO_COMPANION_LIGHT_SLEEP_SECS);
    }
#endif
  }

#if defined(ESP32) && defined(WIFI_SSID) && !defined(WIFI_AP_MODE)
  // Safely attempt to reconnect every 10 seconds if flagged
  if (wifi_needs_reconnect && (millis() - last_wifi_reconnect_attempt > 10000)) {
    WIFI_DEBUG_PRINTLN("Attempting manual WiFi reconnect...");
    WiFi.disconnect();
    WiFi.reconnect();
    last_wifi_reconnect_attempt = millis();
  }
#endif
#if defined(ESP32) && defined(NESSO_SMART_COMPANION)
  nessoMaybeRebootForModeChange();
#endif
}
