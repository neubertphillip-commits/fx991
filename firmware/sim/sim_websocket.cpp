// WebSocket-Client fuer den Simulator (POSIX-Sockets, nicht blockierend im Betrieb).
#include <WebSocketsClient.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "Arduino.h"

#ifndef MSG_NOSIGNAL  // macOS: SIGPIPE wird in sim_main.cpp ignoriert
#define MSG_NOSIGNAL 0
#endif

void WebSocketsClient::begin(const char* host, uint16_t port, const char* url) {
  host_ = host;
  port_ = port;
  url_ = url;
  begun_ = true;
  tried_ = false;
}

void WebSocketsClient::emit(WStype_t type, uint8_t* data, size_t len) {
  if (handler_) handler_(type, data, len);
}

bool WebSocketsClient::connectNow() {
  addrinfo hints = {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  char portStr[8];
  snprintf(portStr, sizeof(portStr), "%u", port_);
  if (getaddrinfo(host_.c_str(), portStr, &hints, &res) != 0) return false;

  int fd = -1;
  for (addrinfo* ai = res; ai; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) continue;
    if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  if (fd < 0) return false;

  // Handshake blockierend mit Timeout
  timeval tv = {3, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  char req[512];
  int n = snprintf(req, sizeof(req),
                   "GET %s HTTP/1.1\r\nHost: %s:%u\r\nUpgrade: websocket\r\n"
                   "Connection: Upgrade\r\nSec-WebSocket-Key: Y2FzaW8tZGVjay1zaW11bA==\r\n"
                   "Sec-WebSocket-Version: 13\r\n\r\n",
                   url_.c_str(), host_.c_str(), port_);
  if (send(fd, req, n, MSG_NOSIGNAL) != n) {
    close(fd);
    return false;
  }
  std::string resp;
  char buf[1024];
  size_t headerEnd = std::string::npos;
  while (headerEnd == std::string::npos) {
    ssize_t r = recv(fd, buf, sizeof(buf), 0);
    if (r <= 0) {
      close(fd);
      return false;
    }
    resp.append(buf, r);
    headerEnd = resp.find("\r\n\r\n");
  }
  if (resp.compare(0, 12, "HTTP/1.1 101") != 0) {
    close(fd);
    return false;
  }
  rx_.assign(resp.begin() + headerEnd + 4, resp.end());
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  fd_ = fd;
  msg_.clear();
  emit(WStype_CONNECTED, reinterpret_cast<uint8_t*>(&url_[0]), url_.size());
  return true;
}

void WebSocketsClient::closeConn() {
  if (fd_ < 0) return;
  close(fd_);
  fd_ = -1;
  rx_.clear();
  emit(WStype_DISCONNECTED, nullptr, 0);
}

void WebSocketsClient::disconnect() {
  if (fd_ >= 0) sendFrame(0x8, nullptr, 0);
  closeConn();
  begun_ = false;
}

void WebSocketsClient::loop() {
  if (!begun_) return;
  if (fd_ < 0) {
    if (tried_ && millis() - lastAttempt_ < reconnectMs_) return;
    tried_ = true;
    lastAttempt_ = millis();
    connectNow();
    return;
  }

  uint8_t buf[4096];
  for (;;) {
    ssize_t r = recv(fd_, buf, sizeof(buf), 0);
    if (r > 0) {
      rx_.insert(rx_.end(), buf, buf + r);
      continue;
    }
    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
    closeConn();  // 0 = Gegenseite hat geschlossen, <0 = Fehler
    return;
  }

  // Frames zerlegen
  while (rx_.size() >= 2) {
    bool fin = rx_[0] & 0x80;
    uint8_t op = rx_[0] & 0x0F;
    bool masked = rx_[1] & 0x80;
    uint64_t len = rx_[1] & 0x7F;
    size_t hdr = 2;
    if (len == 126) {
      if (rx_.size() < 4) return;
      len = (rx_[2] << 8) | rx_[3];
      hdr = 4;
    } else if (len == 127) {
      if (rx_.size() < 10) return;
      len = 0;
      for (int i = 0; i < 8; i++) len = (len << 8) | rx_[2 + i];
      hdr = 10;
    }
    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
      if (rx_.size() < hdr + 4) return;
      memcpy(mask, &rx_[hdr], 4);
      hdr += 4;
    }
    if (rx_.size() < hdr + len) return;
    std::vector<uint8_t> payload(rx_.begin() + hdr, rx_.begin() + hdr + len);
    rx_.erase(rx_.begin(), rx_.begin() + hdr + len);
    if (masked) {
      for (size_t i = 0; i < payload.size(); i++) payload[i] ^= mask[i % 4];
    }

    switch (op) {
      case 0x0:  // Fortsetzung
      case 0x1:  // Text
      case 0x2:  // Binaer
        if (op != 0x0) {
          msgOp_ = op;
          msg_.clear();
        }
        msg_.insert(msg_.end(), payload.begin(), payload.end());
        if (fin) {
          msg_.push_back(0);  // wie links2004: Text ist nullterminiert
          emit(msgOp_ == 0x1 ? WStype_TEXT : WStype_BIN, msg_.data(), msg_.size() - 1);
          msg_.clear();
        }
        break;
      case 0x8:  // Close
        sendFrame(0x8, nullptr, 0);
        closeConn();
        return;
      case 0x9:  // Ping
        sendFrame(0xA, payload.data(), payload.size());
        break;
      default:  // Pong u.a.
        break;
    }
  }
}

bool WebSocketsClient::sendFrame(uint8_t opcode, const uint8_t* data, size_t len) {
  if (fd_ < 0) return false;
  std::vector<uint8_t> frame;
  frame.push_back(0x80 | opcode);
  if (len < 126) {
    frame.push_back(0x80 | len);
  } else if (len < 65536) {
    frame.push_back(0x80 | 126);
    frame.push_back(len >> 8);
    frame.push_back(len & 0xFF);
  } else {
    frame.push_back(0x80 | 127);
    for (int i = 7; i >= 0; i--) frame.push_back((static_cast<uint64_t>(len) >> (8 * i)) & 0xFF);
  }
  uint8_t mask[4];
  for (auto& m : mask) m = rand() & 0xFF;
  frame.insert(frame.end(), mask, mask + 4);
  for (size_t i = 0; i < len; i++) frame.push_back(data[i] ^ mask[i % 4]);

  size_t off = 0;
  while (off < frame.size()) {
    ssize_t w = send(fd_, frame.data() + off, frame.size() - off, MSG_NOSIGNAL);
    if (w > 0) {
      off += w;
    } else if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      pollfd p = {fd_, POLLOUT, 0};
      poll(&p, 1, 1000);
    } else {
      closeConn();
      return false;
    }
  }
  return true;
}

bool WebSocketsClient::sendTXT(const char* payload, size_t length) {
  if (length == 0) length = strlen(payload);
  return sendFrame(0x1, reinterpret_cast<const uint8_t*>(payload), length);
}

bool WebSocketsClient::sendBIN(const uint8_t* payload, size_t length) {
  return sendFrame(0x2, payload, length);
}
