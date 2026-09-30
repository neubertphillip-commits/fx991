#include "net.h"

#include <Arduino.h>
#include <string.h>

#include <vector>

#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#ifdef ESP_PLATFORM
#include <esp_wifi.h>
#endif

#include "config.h"
#include "credentials.h"
#include "screen.h"


namespace {

WebSocketsClient ws;
net::MessageHandler handler = nullptr;
net::BinaryHandler binaryHandler = nullptr;
bool wantOn = false;
bool wsStarted = false;
bool wsConnected = false;
uint32_t connectStart = 0;
const char* bridgeHost = BRIDGE_HOST;  // leer = Gateway (das Handy mit dem Hotspot)
char activeHost[40] = "";              // tatsaechlich verwendete Adresse
uint16_t bridgePort = BRIDGE_PORT;
bool fastAttempt = false;
bool deepPs = false;   // sparsamer Wartemodus des Funkmoduls aktiv
bool lowPowerOk = false;
uint32_t lastTx = 0;   // letztes Senden an die Bridge, fuer WIFI_ACTIVE_MS

// Kanal und Zugangspunkt der letzten Verbindung (bleiben im Tiefschlaf erhalten). Damit
// entfaellt beim naechsten Verbinden die Kanalsuche, das spart Zeit und Strom.
RTC_DATA_ATTR uint8_t savedBssid[6];
RTC_DATA_ATTR int32_t savedChannel = 0;

void noteTx();

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      noteTx();  // gleich wird gesendet: noch nicht in den Wartemodus
      Serial.printf("[net] Bridge verbunden (%s:%u)\n", activeHost, bridgePort);
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
      uint32_t size = doc["size"] | 0u;
      if (handler) handler(t, text, size);
      break;
    }
    case WStype_BIN:
      if (binaryHandler) binaryHandler(payload, length);
      break;
    default:
      break;
  }
}

// Modem-Sleep: Zwischen den Beacons ist das Funkmodul aus. Normal wacht es zu jedem
// DTIM-Beacon auf, im Wartemodus nur alle WIFI_LISTEN_INTERVAL Beacons.
void setPowerSave(bool deep) {
  if (deep == deepPs) return;
  deepPs = deep;
  WiFi.setSleep(deep ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM);
  Serial.println(deep ? "[net] Funk im Wartemodus" : "[net] Funk voll aktiv");
}

void connectSta(int32_t channel, const uint8_t* bssid) {
  WiFi.begin(WIFI_SSID, WIFI_PASS, channel, bssid, false);
#ifdef ESP_PLATFORM
  // Das Abhoerintervall muss vor dem Verbinden gesetzt sein (der Hotspot erfaehrt es
  // bei der Anmeldung); WiFi.begin() kennt keinen Parameter dafuer.
  wifi_config_t conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
    conf.sta.listen_interval = WIFI_LISTEN_INTERVAL;
    esp_wifi_set_config(WIFI_IF_STA, &conf);
  }
  esp_wifi_connect();
#endif
  connectStart = millis();
}

void startWifi() {
  Serial.printf("[net] WLAN an, verbinde mit '%s'\n", WIFI_SSID);
  deepPs = false;
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
  WiFi.mode(WIFI_STA);
  fastAttempt = savedChannel > 0;
  if (fastAttempt) connectSta(savedChannel, savedBssid);
  else connectSta(0, nullptr);
}

void noteTx() {
  lastTx = millis();
  setPowerSave(false);
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
  noteTx();
  char out[Screen::INPUT_BYTES * 2 + 64];  // Platz fuer JSON-Escapes
  size_t len = serializeJson(doc, out, sizeof(out));
  return len > 0 && len < sizeof(out) - 1 && ws.sendTXT(out, len);
}

}  // namespace

namespace net {

void begin(MessageHandler h, BinaryHandler b) {
  handler = h;
  binaryHandler = b;
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

void allowLowPower(bool allowed) { lowPowerOk = allowed; }

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
    if (fastAttempt && millis() - connectStart > WIFI_FAST_TIMEOUT_MS) {
      // Hotspot hat vermutlich den Kanal gewechselt: normal suchen
      Serial.println("[net] Schnellverbindung klappt nicht, suche Hotspot");
      fastAttempt = false;
      savedChannel = 0;
      WiFi.disconnect();
      connectSta(0, nullptr);
    } else if (millis() - connectStart > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println("[net] WLAN-Timeout, neuer Versuch");
      WiFi.disconnect();
      connectSta(0, nullptr);
    }
    return;
  }

  if (!wsStarted) {
    Serial.printf("[net] WLAN verbunden nach %u ms, IP %s\n",
                  static_cast<unsigned>(millis() - connectStart), WiFi.localIP().toString().c_str());
    if (const uint8_t* bssid = WiFi.BSSID()) {
      memcpy(savedBssid, bssid, sizeof(savedBssid));
      savedChannel = WiFi.channel();
    }
    // Die Bridge laeuft auf dem Handy, das den Hotspot macht: dessen Adresse ist das
    // Gateway, auch wenn Android das Hotspot-Netz bei jedem Einschalten neu waehlt.
    if (bridgeHost[0]) snprintf(activeHost, sizeof(activeHost), "%s", bridgeHost);
    else snprintf(activeHost, sizeof(activeHost), "%s", WiFi.gatewayIP().toString().c_str());
    ws.begin(activeHost, bridgePort, "/");
    ws.onEvent(onWsEvent);
    ws.setReconnectInterval(3000);
    ws.enableHeartbeat(15000, 3000, 2);  // WebSocket-Ping, erkennt tote Verbindungen
    wsStarted = true;
    noteTx();  // Verbindungsaufbau mit vollem Tempo
  }
  ws.loop();
  if (!lowPowerOk) setPowerSave(false);
  else if (wsConnected && millis() - lastTx > WIFI_ACTIVE_MS) setPowerSave(true);
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
  noteTx();
  return ws.sendBIN(data, len);
}

bool sendSync(const FileInfo* files, size_t count, uint32_t freeBytes) {
  if (!wsConnected) return false;
  JsonDocument doc;
  doc["t"] = "sync";
  doc["free"] = freeBytes;
  JsonArray have = doc["have"].to<JsonArray>();
  for (size_t i = 0; i < count; i++) {
    JsonArray f = have.add<JsonArray>();
    f.add(files[i].name);
    f.add(files[i].size);
    f.add(files[i].crc);
  }
  // Die Liste kann lang werden: Puffer passend auf dem Heap statt fester Groesse
  std::vector<char> out(measureJson(doc) + 1);
  size_t len = serializeJson(doc, out.data(), out.size());
  noteTx();
  return len > 0 && ws.sendTXT(out.data(), len);
}

}  // namespace net
