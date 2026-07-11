#include "SerialWifiInterface.h"
#include <WiFi.h>

bool SerialWifiInterface::pushQueue(Frame queue[], uint8_t head, uint8_t& len, const uint8_t src[], size_t src_len) {
  if (len >= WIFI_FRAME_QUEUE_SIZE || src_len > MAX_FRAME_SIZE) return false;

  uint8_t idx = (head + len) % WIFI_FRAME_QUEUE_SIZE;
  queue[idx].len = (uint8_t)src_len;
  memcpy(queue[idx].buf, src, src_len);
  len++;
  return true;
}

bool SerialWifiInterface::popQueue(Frame queue[], uint8_t& head, uint8_t& len, Frame& dest) {
  if (len == 0) return false;

  dest = queue[head];
  head = (head + 1) % WIFI_FRAME_QUEUE_SIZE;
  len--;
  return true;
}

static bool drainClientBytes(WiFiClient& client, int frame_length) {
  uint8_t skip[16];
  while (frame_length > 0) {
    int to_read = frame_length < (int)sizeof(skip) ? frame_length : (int)sizeof(skip);
    int skipped = client.read(skip, to_read);
    if (skipped <= 0) return false;
    frame_length -= skipped;
  }
  return true;
}

void SerialWifiInterface::begin(int port) {
  // wifi setup is handled outside of this class, only starts the server
  server.begin(port);
}

// ---------- public methods
void SerialWifiInterface::enable() { 
  if (_isEnabled) return;

  _isEnabled = true;
  clearBuffers();
}

void SerialWifiInterface::disable() {
  _isEnabled = false;
  deviceConnected = false;
  client.stop();
  resetReceivedFrameHeader();
  clearBuffers();
}

size_t SerialWifiInterface::writeFrame(const uint8_t src[], size_t len) {
  if (!_isEnabled) return 0;

  if (len > MAX_FRAME_SIZE) {
    WIFI_DEBUG_PRINTLN("writeFrame(), frame too big, len=%d\n", len);
    return 0;
  }

  if (deviceConnected && len > 0) {
    if (!pushQueue(send_queue, send_queue_head, send_queue_len, src, len)) {
      WIFI_DEBUG_PRINTLN("writeFrame(), send_queue is full!");
      return 0;
    }

    return len;
  }
  return 0;
}

bool SerialWifiInterface::isWriteBusy() const {
  return false;
}

bool SerialWifiInterface::hasReceivedFrameHeader() {
  return received_frame_header.type != 0 && received_frame_header.length != 0;
}

void SerialWifiInterface::resetReceivedFrameHeader() {
  received_frame_header.type = 0;
  received_frame_header.length = 0;
}

size_t SerialWifiInterface::checkRecvFrame(uint8_t dest[]) {
  if (!_isEnabled) return 0;

  // check if new client connected
  auto newClient = server.available();
  if (newClient) {

    // disconnect existing client
    deviceConnected = false;
    client.stop();

    // switch active connection to new client
    client = newClient;

    // forget received frame header
    resetReceivedFrameHeader();
    
  }

  if (client.connected()) {
    if (!deviceConnected) {
      WIFI_DEBUG_PRINTLN("Got connection");
      deviceConnected = true;
    }
  } else {
    if (deviceConnected) {
      deviceConnected = false;
      WIFI_DEBUG_PRINTLN("Disconnected");
    }
  }

  if (deviceConnected) {
    if (send_queue_len > 0) {   // first, check send queue
      Frame frame;
      if (!popQueue(send_queue, send_queue_head, send_queue_len, frame)) return 0;
      
      _last_write = millis();
      int len = frame.len;

      uint8_t pkt[3+len]; // use same header as serial interface so client can delimit frames
      pkt[0] = '>';
      pkt[1] = (len & 0xFF);  // LSB
      pkt[2] = (len >> 8);    // MSB
      memcpy(&pkt[3], frame.buf, frame.len);
      client.write(pkt, 3 + len);
    } else {

      // check if we are waiting for a frame header
      if(!hasReceivedFrameHeader()){

        // make sure we have received enough bytes for a frame header
        // 3 bytes frame header = (1 byte frame type) + (2 bytes frame length as unsigned 16-bit little endian)
        int frame_header_length = 3;
        if(client.available() >= frame_header_length){

          // read frame header
          client.readBytes(&received_frame_header.type, 1);
          client.readBytes((uint8_t*)&received_frame_header.length, 2);

        }

      }

      // check if we have received a frame header
      if(hasReceivedFrameHeader()){

        // make sure we have received enough bytes for the required frame length
        int available = client.available();
        int frame_type = received_frame_header.type;
        int frame_length = received_frame_header.length;
        if(frame_length > available){
          WIFI_DEBUG_PRINTLN("Waiting for %d more bytes", frame_length - available);
          return 0;
        }

        // skip frames that are larger than MAX_FRAME_SIZE
        if(frame_length > MAX_FRAME_SIZE){
          WIFI_DEBUG_PRINTLN("Skipping frame: length=%d is larger than MAX_FRAME_SIZE=%d", frame_length, MAX_FRAME_SIZE);
          if (!drainClientBytes(client, frame_length)) {
            WIFI_DEBUG_PRINTLN("Unable to drain oversized frame; closing client");
            deviceConnected = false;
            client.stop();
            clearBuffers();
          }
          resetReceivedFrameHeader();
          return 0;
        }

        // skip frames that are not expected type
        // '<' is 0x3c which indicates a frame sent from app to radio
        if(frame_type != '<'){
          WIFI_DEBUG_PRINTLN("Skipping frame: type=0x%x is unexpected", frame_type);
          if (!drainClientBytes(client, frame_length)) {
            WIFI_DEBUG_PRINTLN("Unable to drain unexpected frame; closing client");
            deviceConnected = false;
            client.stop();
            clearBuffers();
          }
          resetReceivedFrameHeader();
          return 0;
        }

        // read frame data to provided buffer
        client.readBytes(dest, frame_length);

        // ready for next frame
        resetReceivedFrameHeader();
        return frame_length;

      }
      
    }
  }

  return 0;
}

bool SerialWifiInterface::isConnected() const {
  return _isEnabled && deviceConnected;  //pServer != NULL && pServer->getConnectedCount() > 0;
}
