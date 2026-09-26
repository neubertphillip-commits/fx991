#include "net.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <WiFi.h>

#include "config.h"
#include "credentials.h"
#include "screen.h"


namespace {

WebSocketsClient ws;
net::MessageHandler handler = nullptr;
bool wantOn = false;
bool wsStarted = false;
bool wsConnected = false;
uint32_t connectStart = 0;
const char* bridgeHost = BRIDGE_HOST;
uint16_t bridgePort = BRIDGE_PORT;

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      Serial.printf("[net] Bridge verbunden (%s:%u)\n", bridgeHost, bridgePort);
      break;
    case WStype_DISCONNECTED:
      if (wsConnected) Serial.println("[net] Bridge getrennt");
      wsConnected = false;
      break;
    case WStype_TEXT: {
      JsonDocument doc;
      if (deserializeJson(doc, payload, length)) {
        Serial.println("[net] ungueltiges JSON von der Bridge");
        break;
      }
      const char* t = doc["t"] | "";
      const char* text = doc["text"] | "";
      if (handler) handler(t, text);
      break;
    }
    default:
      break;
  }
}

void startWifi() {
  Serial.printf("[net] WLAN an, verbinde mit '%s'\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);  // Modem-Sleep zwischen den Beacons spart Strom
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  connectStart = millis();
}

void stopAll() {
  if (wsStarted) {
    ws.disconnect();
    wsStarted = false;
  }
  wsConnected = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[net] WLAN aus");
}

bool sendJson(JsonDocument& doc) {
  if (!wsConnected) return false;
  char out[Screen::INPUT_BYTES * 2 + 64];  // Platz fuer JSON-Escapes
  size_t len = serializeJson(doc, out, sizeof(out));
  return len > 0 && len < sizeof(out) - 1 && ws.sendTXT(out, len);
}

}  // namespace

namespace net {

void begin(MessageHandler h) {
  handler = h;
  WiFi.persistent(false);  // Zugangsdaten nicht bei jedem begin() ins Flash schreiben
  WiFi.mode(WIFI_OFF);
}

void enable(bool on) {
  if (on == wantOn) return;
  wantOn = on;
  if (on) startWifi();
  else stopAll();
}

bool enabled() { return wantOn; }

void setBridge(const char* host, uint16_t port) {
  bridgeHost = host;
  bridgePort = port;
}

State state() {
  if (!wantOn) return State::Off;
  if (WiFi.status() != WL_CONNECTED) return State::Connecting;
  return wsConnected ? State::Online : State::WifiUp;
}

const char* stateName(State s) {
  switch (s) {
    case State::Off: return "aus";
    case State::Connecting: return "verbinde";
    case State::WifiUp: return "WLAN";
    case State::Online: return "online";
  }
  return "?";
}

void loop() {
  if (!wantOn) return;

  if (WiFi.status() != WL_CONNECTED) {
    if (wsStarted) {
      ws.disconnect();
      wsStarted = false;
      wsConnected = false;
    }
    if (millis() - connectStart > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println("[net] WLAN-Timeout, neuer Versuch");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      connectStart = millis();
    }
    return;
  }

  if (!wsStarted) {
    Serial.printf("[net] WLAN verbunden, IP %s\n", WiFi.localIP().toString().c_str());
    ws.begin(bridgeHost, bridgePort, "/");
    ws.onEvent(onWsEvent);
    ws.setReconnectInterval(3000);
    ws.enableHeartbeat(15000, 3000, 2);  // WebSocket-Ping, erkennt tote Verbindungen
    wsStarted = true;
  }
  ws.loop();
}

bool sendPrompt(const char* text) {
  JsonDocument doc;
  doc["t"] = "prompt";
  doc["text"] = text;
  return sendJson(doc);
}

bool sendNew() {
  JsonDocument doc;
  doc["t"] = "new";
  return sendJson(doc);
}

bool sendPing() {
  JsonDocument doc;
  doc["t"] = "ping";
  return sendJson(doc);
}

bool sendBinary(const uint8_t* data, size_t len) {
  if (!wsConnected) return false;
  return ws.sendBIN(data, len);
}

}  // namespace net
