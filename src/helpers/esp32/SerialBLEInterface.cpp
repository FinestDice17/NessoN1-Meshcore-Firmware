#include "SerialBLEInterface.h"
#include "esp_mac.h"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define ADVERT_RESTART_DELAY  1000   // millis

#ifndef NESSO_CONNECTIVITY_LOGGING
  #define NESSO_CONNECTIVITY_LOGGING 0
#endif

#if NESSO_CONNECTIVITY_LOGGING && ARDUINO
  #define BLE_CONN_LOG(F, ...) Serial.printf("Nesso: BLE " F "\n", ##__VA_ARGS__)
#else
  #define BLE_CONN_LOG(...) {}
#endif

#ifndef BLE_MTU_SIZE
  #define BLE_MTU_SIZE (MAX_FRAME_SIZE + 3)
#endif

#ifndef BLE_REQUIRE_MITM
  #define BLE_REQUIRE_MITM 0
#endif

#ifndef BLE_ENABLE_BONDING
  #define BLE_ENABLE_BONDING 0
#endif

#ifndef BLE_OPEN_GATT
  #define BLE_OPEN_GATT 0
#endif

#if BLE_REQUIRE_MITM && BLE_ENABLE_BONDING
  #define BLE_AUTH_MODE ESP_LE_AUTH_REQ_SC_MITM_BOND
#elif BLE_REQUIRE_MITM
  #define BLE_AUTH_MODE ESP_LE_AUTH_REQ_SC_MITM
#elif BLE_ENABLE_BONDING
  #define BLE_AUTH_MODE ESP_LE_AUTH_REQ_SC_BOND
#else
  #define BLE_AUTH_MODE ESP_LE_AUTH_REQ_SC_ONLY
#endif

bool SerialBLEInterface::pushQueue(Frame queue[], uint8_t head, uint8_t& len, const uint8_t src[], size_t src_len) {
  if (len >= BLE_FRAME_QUEUE_SIZE || src_len > MAX_FRAME_SIZE) return false;

  uint8_t idx = (head + len) % BLE_FRAME_QUEUE_SIZE;
  queue[idx].len = (uint8_t)src_len;
  memcpy(queue[idx].buf, src, src_len);
  len++;
  return true;
}

bool SerialBLEInterface::popQueue(Frame queue[], uint8_t& head, uint8_t& len, Frame& dest) {
  if (len == 0) return false;

  dest = queue[head];
  head = (head + 1) % BLE_FRAME_QUEUE_SIZE;
  len--;
  return true;
}

void SerialBLEInterface::begin(const char* prefix, char* name, uint32_t pin_code) {
  _pin_code = pin_code;

  if (strcmp(name, "@@MAC") == 0) {
    uint8_t addr[8];
    memset(addr, 0, sizeof(addr));
    esp_efuse_mac_get_default(addr);
    sprintf(name, "%02X%02X%02X%02X%02X%02X",    // modify (IN-OUT param)
          addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
  }
  char dev_name[32+16];
  sprintf(dev_name, "%s%s", prefix, name);

  // Create the BLE Device
  BLEDevice::init(dev_name);
  BLE_DEBUG_PRINTLN("begin device=%s", dev_name);
#if !BLE_OPEN_GATT
  BLEDevice::setSecurityCallbacks(this);
#endif
  BLEDevice::setMTU(BLE_MTU_SIZE);

#if !BLE_OPEN_GATT
  BLESecurity  sec;
  sec.setStaticPIN(pin_code);
  sec.setAuthenticationMode(BLE_AUTH_MODE);
#endif

  //BLEDevice::setPower(ESP_PWR_LVL_N8);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(this);

  // Create the BLE Service
  pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
#if BLE_OPEN_GATT
  pTxCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ);
#elif BLE_REQUIRE_MITM
  pTxCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);
#else
  pTxCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
#endif
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
#if BLE_OPEN_GATT
  pRxCharacteristic->setAccessPermissions(ESP_GATT_PERM_WRITE);
#elif BLE_REQUIRE_MITM
  pRxCharacteristic->setAccessPermissions(ESP_GATT_PERM_WRITE_ENC_MITM);
#else
  pRxCharacteristic->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
#endif
  pRxCharacteristic->setCallbacks(this);

  pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);
}

// -------- BLESecurityCallbacks methods

uint32_t SerialBLEInterface::onPassKeyRequest() {
  BLE_DEBUG_PRINTLN("onPassKeyRequest()");
  return _pin_code;
}

void SerialBLEInterface::onPassKeyNotify(uint32_t pass_key) {
  BLE_DEBUG_PRINTLN("onPassKeyNotify(%u)", pass_key);
}

bool SerialBLEInterface::onConfirmPIN(uint32_t pass_key) {
  BLE_DEBUG_PRINTLN("onConfirmPIN(%u)", pass_key);
  return true;
}

bool SerialBLEInterface::onSecurityRequest() {
  BLE_DEBUG_PRINTLN("onSecurityRequest()");
  return true;  // allow
}

void SerialBLEInterface::onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
  if (!_isEnabled) {
    deviceConnected = false;
    return;
  }

  if (cmpl.success) {
    BLE_DEBUG_PRINTLN(" - SecurityCallback - Authentication Success");
    deviceConnected = true;
  } else {
    BLE_DEBUG_PRINTLN(" - SecurityCallback - Authentication Failure*");

    //pServer->removePeerDevice(pServer->getConnId(), true);
    pServer->disconnect(pServer->getConnId());
    adv_restart_time = millis() + ADVERT_RESTART_DELAY;
  }
}

// -------- BLEServerCallbacks methods

void SerialBLEInterface::onConnect(BLEServer* pServer) {
}

void SerialBLEInterface::onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
  BLE_DEBUG_PRINTLN("onConnect(), conn_id=%d, mtu=%d", param->connect.conn_id, pServer->getPeerMTU(param->connect.conn_id));
  BLE_CONN_LOG("connected conn_id=%d mtu=%d", param->connect.conn_id, pServer->getPeerMTU(param->connect.conn_id));
  last_conn_id = param->connect.conn_id;
  if (!_isEnabled) {
    pServer->disconnect(last_conn_id);
    return;
  }
#if BLE_OPEN_GATT
  deviceConnected = true;
#endif
}

void SerialBLEInterface::onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
  BLE_DEBUG_PRINTLN("onMtuChanged(), mtu=%d", pServer->getPeerMTU(param->mtu.conn_id));
}

void SerialBLEInterface::onDisconnect(BLEServer* pServer) {
  BLE_DEBUG_PRINTLN("onDisconnect()");
  BLE_CONN_LOG("disconnected");
  deviceConnected = false;
  if (_isEnabled) {
    adv_restart_time = millis() + ADVERT_RESTART_DELAY;

    // loop() will detect this on next loop, and set deviceConnected to false
  }
}

// -------- BLECharacteristicCallbacks methods

void SerialBLEInterface::onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) {
  if (!_isEnabled) return;

  uint8_t* rxValue = pCharacteristic->getData();
  int len = pCharacteristic->getLength();

  if (len > MAX_FRAME_SIZE) {
    BLE_DEBUG_PRINTLN("ERROR: onWrite(), frame too big, len=%d", len);
  } else if (!pushQueue(recv_queue, recv_queue_head, recv_queue_len, rxValue, len)) {
    BLE_DEBUG_PRINTLN("ERROR: onWrite(), recv_queue is full!");
  }
}

// ---------- public methods

void SerialBLEInterface::enable() { 
  if (_isEnabled) return;

  _isEnabled = true;
  clearBuffers();

  // Start the service
  pService->start();

  // Start advertising

  //pServer->getAdvertising()->setMinInterval(500);
  //pServer->getAdvertising()->setMaxInterval(1000);

  pServer->getAdvertising()->start();
  BLE_DEBUG_PRINTLN("advertising started");
  BLE_CONN_LOG("advertising started");
  adv_restart_time = 0;
}

void SerialBLEInterface::disable() {
  _isEnabled = false;

  BLE_DEBUG_PRINTLN("SerialBLEInterface::disable");

  if (pServer) {
    pServer->getAdvertising()->stop();
    pServer->disconnect(last_conn_id);
  }
  if (pService) pService->stop();
  oldDeviceConnected = deviceConnected = false;
  adv_restart_time = 0;
  clearBuffers();
}

size_t SerialBLEInterface::writeFrame(const uint8_t src[], size_t len) {
  if (!_isEnabled) return 0;

  if (len > MAX_FRAME_SIZE) {
    BLE_DEBUG_PRINTLN("writeFrame(), frame too big, len=%d", len);
    return 0;
  }

  if (deviceConnected && len > 0) {
    if (!pushQueue(send_queue, send_queue_head, send_queue_len, src, len)) {
      BLE_DEBUG_PRINTLN("writeFrame(), send_queue is full!");
      return 0;
    }

    return len;
  }
  return 0;
}

#define  BLE_WRITE_MIN_INTERVAL   60

bool SerialBLEInterface::isWriteBusy() const {
  return millis() < _last_write + BLE_WRITE_MIN_INTERVAL;   // still too soon to start another write?
}

size_t SerialBLEInterface::checkRecvFrame(uint8_t dest[]) {
  if (!_isEnabled) return 0;

  if (send_queue_len > 0   // first, check send queue
    && millis() >= _last_write + BLE_WRITE_MIN_INTERVAL    // space the writes apart
  ) {
    Frame frame;
    if (!popQueue(send_queue, send_queue_head, send_queue_len, frame)) return 0;

    _last_write = millis();
    pTxCharacteristic->setValue(frame.buf, frame.len);
    pTxCharacteristic->notify();

    BLE_DEBUG_PRINTLN("writeBytes: sz=%d, hdr=%d", (uint32_t)frame.len, (uint32_t) frame.buf[0]);
  }

  if (recv_queue_len > 0) {   // check recv queue
    Frame frame;
    if (!popQueue(recv_queue, recv_queue_head, recv_queue_len, frame)) return 0;
    size_t len = frame.len;
    memcpy(dest, frame.buf, len);

    BLE_DEBUG_PRINTLN("readBytes: sz=%d, hdr=%d", len, (uint32_t) dest[0]);
    return len;
  }

  if (pServer->getConnectedCount() == 0)  deviceConnected = false;

  if (deviceConnected != oldDeviceConnected) {
    if (!deviceConnected) {    // disconnecting
      clearBuffers();

      BLE_DEBUG_PRINTLN("SerialBLEInterface -> disconnecting...");

      //pServer->getAdvertising()->setMinInterval(500);
      //pServer->getAdvertising()->setMaxInterval(1000);

      adv_restart_time = millis() + ADVERT_RESTART_DELAY;
    } else {
      BLE_DEBUG_PRINTLN("SerialBLEInterface -> stopping advertising");
      BLE_DEBUG_PRINTLN("SerialBLEInterface -> connecting...");
      // connecting
      // do stuff here on connecting
      pServer->getAdvertising()->stop();
      adv_restart_time = 0;
    }
    oldDeviceConnected = deviceConnected;
  }

  if (adv_restart_time && millis() >= adv_restart_time) {
    if (pServer->getConnectedCount() == 0) {
      BLE_DEBUG_PRINTLN("SerialBLEInterface -> re-starting advertising");
      pServer->getAdvertising()->start();  // re-Start advertising
    }
    adv_restart_time = 0;
  }
  return 0;
}

bool SerialBLEInterface::isConnected() const {
  return _isEnabled && deviceConnected;  //pServer != NULL && pServer->getConnectedCount() > 0;
}
