// Kleiner WebSocket-Client (RFC 6455) mit derselben Schnittstelle wie
// links2004/WebSockets, soweit net.cpp sie nutzt. Nur fuer den PC-Simulator.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

enum WStype_t {
  WStype_ERROR,
  WStype_DISCONNECTED,
  WStype_CONNECTED,
  WStype_TEXT,
  WStype_BIN,
};

class WebSocketsClient {
 public:
  using Handler = void (*)(WStype_t type, uint8_t* payload, size_t length);

  void begin(const char* host, uint16_t port, const char* url = "/");
  void onEvent(Handler h) { handler_ = h; }
  void setReconnectInterval(unsigned long ms) { reconnectMs_ = ms; }
  void enableHeartbeat(uint32_t, uint32_t, uint8_t) {}
  void loop();
  void disconnect();

  bool sendTXT(const char* payload, size_t length = 0);
  bool sendBIN(const uint8_t* payload, size_t length);

 private:
  bool connectNow();
  void closeConn();
  bool sendFrame(uint8_t opcode, const uint8_t* data, size_t len);
  void emit(WStype_t type, uint8_t* data, size_t len);

  Handler handler_ = nullptr;
  std::string host_, url_;
  uint16_t port_ = 0;
  bool begun_ = false;
  int fd_ = -1;
  unsigned long reconnectMs_ = 1000;
  uint32_t lastAttempt_ = 0;
  bool tried_ = false;
  std::vector<uint8_t> rx_;   // empfangene, noch nicht verarbeitete Bytes
  std::vector<uint8_t> msg_;  // Nachricht aus Fragmenten
  uint8_t msgOp_ = 0;
};
