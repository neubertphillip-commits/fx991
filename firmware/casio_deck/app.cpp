#include "app.h"

#include <Arduino.h>
#include <string.h>
#include <strings.h>

#include "calc.h"
#include "camera.h"
#include "config.h"
#include "display.h"
#include "keypad.h"
#include "mic.h"
#include "multitap.h"
#include "net.h"
#include "ota.h"
#include "power.h"
#include "screen.h"

namespace {

// Rechner: offline, WLAN aus. Terminal: Prompts an die Bridge (claude -p).
// Kamera: Foto (+ optional Frage) an die Bridge.
// Spracheingabe (Terminal/Kamera): SHIFT+ALPHA startet, EXE oder SHIFT+ALPHA schickt,
// AC verwirft; die Bridge schickt den erkannten Text zurueck in die Eingabezeile.
// MODE wechselt reihum; auf dem seriellen Monitor auch :calc, :term, :cam.
// SHIFT+AC schaltet aus (Tiefschlaf), eine beliebige Taste wieder ein.
enum class Mode : uint8_t { Calc, Terminal, Camera };
constexpr uint8_t MODE_COUNT = 3;
const char* const MODE_NAMES[MODE_COUNT] = {"RECHNER", "TERMINAL", "KAMERA"};

// RTC_DATA_ATTR: bleibt im Tiefschlaf erhalten (nicht bei leerem Akku)
RTC_DATA_ATTR Mode mode = Mode::Calc;
Screen screens[MODE_COUNT];
// ALPHA-Zustand je Modus: im Terminal und bei der Kamera-Frage meist Text.
RTC_DATA_ATTR bool alpha[MODE_COUNT] = {false, true, true};

bool keypadOk = false;
bool logKeys = false;  // alle Tastenereignisse seriell ausgeben
bool shift = false;
MultiTap multitap;

// Rechner
RTC_DATA_ATTR double ans = 0;
RTC_DATA_ATTR bool degrees = true;
uint32_t calcSince = 0;  // seit wann im Rechnermodus (WLAN-Abschaltung)

// Bridge
bool busy = false;                       // Anfrage laeuft
Mode replyTo = Mode::Terminal;           // wohin die Antwort geschrieben wird
char pending[Screen::INPUT_BYTES] = "";  // Prompt, der auf die Verbindung wartet

// Spracheingabe
bool voiceActive = false;  // Aufnahme laeuft (aus Sicht der App)
bool voiceSend = false;    // erkannten Text nach "done" abschicken (VOICE_AUTO_SEND)

// Ein/Aus
uint32_t lastActivity = 0;  // letzte Eingabe, fuer AUTO_OFF_MS
bool offRequested = false;  // ausschalten, sobald alle Tasten losgelassen sind
bool ignoreKeys = false;    // Taste, die aus dem Tiefschlaf geweckt hat, nicht auswerten

char serialBuf[Screen::INPUT_BYTES];
size_t serialLen = 0;

uint8_t idx(Mode m) { return static_cast<uint8_t>(m); }
Screen& screen(Mode m) { return screens[idx(m)]; }
Screen& screen() { return screen(mode); }

void updateStatus() {
  char buf[Screen::LINE_BYTES];
  const char* netState = net::stateName(net::state());
  char rec[16];
  const char* input = alpha[idx(mode)] ? (shift ? "ABC" : "abc") : (shift ? "S" : "123");
  if (voiceActive) {
    snprintf(rec, sizeof(rec), "REC %us", static_cast<unsigned>(mic::elapsedMs() / 1000));
    input = rec;
  }
  switch (mode) {
    case Mode::Calc:
      snprintf(buf, sizeof(buf), "%s %s | %s | WLAN %s%s", MODE_NAMES[0], input,
               degrees ? "DEG" : "RAD", netState, keypadOk ? "" : " | MCP fehlt");
      break;
    default:
      snprintf(buf, sizeof(buf), "%s %s | Bridge %s%s%s", MODE_NAMES[idx(mode)], input, netState,
               busy ? " | denkt..." : "", keypadOk ? "" : " | MCP fehlt");
      break;
  }
  screen().setStatus(buf);
  screen().setInputMarked(multitap.pending(millis()));
}

void voiceCancel();

void setMode(Mode m) {
  if (voiceActive) voiceCancel();
  if (mode == Mode::Camera && m != Mode::Camera) camera::end();
  mode = m;
  shift = false;
  multitap.reset();
  if (m == Mode::Calc) {
    calcSince = millis();
  } else {
    net::enable(true);
  }
  if (m == Mode::Camera && !camera::begin()) screen().print(camera::error());
  updateStatus();
}

void nextMode() { setMode(static_cast<Mode>((idx(mode) + 1) % MODE_COUNT)); }

// ---------------------------------------------------------------------------
// Bridge
// ---------------------------------------------------------------------------

void submit();

void onBridge(const char* type, const char* text) {
  Screen& s = screen(replyTo);
  lastActivity = millis();
  if (!strcmp(type, "busy")) {
    busy = true;
  } else if (!strcmp(type, "line")) {
    s.print(text);
  } else if (!strcmp(type, "text")) {
    // Erkannte Sprache: an die Eingabe anhaengen, dort kann man sie noch korrigieren
    const char* in = s.input();
    size_t len = strlen(in);
    if (len && in[len - 1] != ' ') s.inputAppend(" ");
    s.inputAppend(text);
    voiceSend = VOICE_AUTO_SEND && replyTo == mode;
  } else if (!strcmp(type, "done")) {
    busy = false;
    calcSince = millis();
    if (voiceSend) {
      voiceSend = false;
      submit();
    }
  } else if (!strcmp(type, "err")) {
    char buf[Screen::LINE_BYTES * 2];
    snprintf(buf, sizeof(buf), "! %s", text);
    s.print(buf);
  } else if (!strcmp(type, "pong")) {
    s.print("pong");
  }
}

// ---------------------------------------------------------------------------
// Spracheingabe
// ---------------------------------------------------------------------------

void voiceStart() {
  Screen& s = screen();
  if (mode == Mode::Calc) {
    s.print("(Spracheingabe nur im Terminal- und Kameramodus)");
    return;
  }
  if (busy) {
    s.print("(warte noch auf die letzte Antwort)");
    return;
  }
  net::enable(true);
  if (!mic::start()) {
    s.print(mic::error());
    return;
  }
  voiceActive = true;
  multitap.reset();
}

void voiceCancel() {
  mic::cancel();
  voiceActive = false;
}

// Aufnahme beenden und als WAV an die Bridge schicken
void voiceFinish() {
  Screen& s = screen();
  mic::stop();
  voiceActive = false;
  const uint8_t* wav;
  size_t len;
  if (!mic::wav(wav, len)) {
    s.print(mic::error());
    return;
  }
  if (net::state() != net::State::Online) {
    s.print("(Bridge nicht verbunden, Aufnahme verworfen)");
  } else if (!net::sendBinary(wav, len)) {
    s.print("! Senden fehlgeschlagen");
  } else {
    replyTo = mode;
    busy = true;
  }
  mic::release();
}

void sendPending() {
  if (!pending[0] || net::state() != net::State::Online) return;
  if (net::sendPrompt(pending)) {
    busy = true;
    pending[0] = '\0';
  }
}

// ---------------------------------------------------------------------------
// Eingabe abschicken (EXE)
// ---------------------------------------------------------------------------

void submitCalc() {
  Screen& s = screen(Mode::Calc);
  const char* expr = s.input();
  if (!expr[0]) return;

  CalcResult r = calcEval(expr, ans, degrees);
  char line[Screen::LINE_BYTES];
  s.print(expr);
  if (r.ok) {
    char num[32];
    calcFormat(r.value, num, sizeof(num));
    snprintf(line, sizeof(line), "%*s", Screen::COLS, num);  // rechtsbuendig wie beim Casio
    s.print(line);
    ans = r.value;
    s.inputClear();
  } else {
    snprintf(line, sizeof(line), "  %s", r.error);
    s.print(line);  // Eingabe bleibt zum Korrigieren stehen
  }
}

void submitTerminal() {
  Screen& s = screen(Mode::Terminal);
  if (!s.input()[0]) return;
  if (busy || pending[0]) {
    s.print("(warte noch auf die letzte Antwort)");
    return;
  }
  char line[Screen::INPUT_BYTES + 2];
  snprintf(line, sizeof(line), "> %s", s.input());
  s.print(line);
  strncpy(pending, s.input(), sizeof(pending) - 1);
  s.inputClear();
  replyTo = Mode::Terminal;
  net::enable(true);
  if (net::state() != net::State::Online) s.print("(wird gesendet, sobald die Bridge verbunden ist)");
  sendPending();
}

// Foto aufnehmen und mit der Eingabe als Frage schicken (leer = "beschreibe das Bild").
void submitCamera() {
  Screen& s = screen(Mode::Camera);
  if (busy) {
    s.print("(warte noch auf die letzte Antwort)");
    return;
  }
  if (net::state() != net::State::Online) {
    s.print("(Bridge nicht verbunden)");
    return;
  }
  const uint8_t* jpeg;
  size_t len;
  if (!camera::capture(jpeg, len)) {
    s.print(camera::error());
    return;
  }
  bool sent = net::sendBinary(jpeg, len);
  camera::release();
  if (!sent || !net::sendPrompt(s.input())) {
    s.print("! Senden fehlgeschlagen");
    return;
  }
  char line[Screen::INPUT_BYTES + 32];
  snprintf(line, sizeof(line), "> [Foto %u kB] %s", static_cast<unsigned>((len + 512) / 1024),
           s.input());
  s.print(line);
  s.inputClear();
  replyTo = Mode::Camera;
  busy = true;
}

void submit() {
  multitap.reset();
  switch (mode) {
    case Mode::Calc: submitCalc(); break;
    case Mode::Terminal: submitTerminal(); break;
    case Mode::Camera: submitCamera(); break;
  }
}

// ---------------------------------------------------------------------------
// Tasten
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Ein/Aus
// ---------------------------------------------------------------------------

// Grund, warum gerade nicht ausgeschaltet werden darf, sonst nullptr.
const char* cannotSleep() {
  if (!keypadOk) return "(ohne Tastatur kein Ausschalten, nichts koennte wecken)";
  if (power::updatePending()) return "(neue Firmware erst bestaetigen: WLAN verbinden)";
  if (ota::running()) return "(Update laeuft)";
  return nullptr;
}

void requestOff() {
  if (const char* why = cannotSleep()) {
    screen().print(why);
    return;
  }
  offRequested = true;  // erst ausschalten, wenn AC losgelassen ist, sonst weckt es sofort
}

void sleepNow() {
  offRequested = false;
  if (voiceActive) voiceCancel();
  camera::end();
  net::enable(false);
  if (!keypad::armWake()) {  // doch noch eine Taste gedrueckt: gleich nochmal versuchen
    offRequested = true;
    return;
  }
  display::power(false);
  power::sleep();  // Hardware: kehrt nicht zurueck, Aufwachen = Neustart

  // nur im PC-Simulator: weiter wie nach dem Aufwachen
  display::power(true);
  lastActivity = millis();
  setMode(mode);
}

// Text, den eine Taste in die Eingabezeile schreibt, sonst nullptr.
const char* keyText(Key k, bool shifted) {
  switch (k) {
    case K_0: return "0";
    case K_1: return "1";
    case K_2: return "2";
    case K_3: return "3";
    case K_4: return "4";
    case K_5: return "5";
    case K_6: return "6";
    case K_7: return "7";
    case K_8: return "8";
    case K_9: return "9";
    case K_DOT: return ".";
    case K_EXP: return "E";
    case K_ANS: return "Ans";
    case K_ADD: return "+";
    case K_SUB: return "-";
    case K_MUL: return "*";
    case K_DIV: return "/";
    case K_POW: return "^";
    case K_LPAR: return "(";
    case K_RPAR: return ")";
    case K_SQRT: return "sqrt(";
    case K_SIN: return shifted ? "asin(" : "sin(";
    case K_COS: return shifted ? "acos(" : "cos(";
    case K_TAN: return shifted ? "atan(" : "tan(";
    case K_LN: return shifted ? "exp(" : "ln(";
    case K_LOG: return "log(";
    default: return nullptr;
  }
}

void onKey(Key k) {
  Screen& s = screen();
  bool wasShift = shift;
  shift = false;

  // Waehrend der Aufnahme: EXE/ALPHA schicken, AC verwirft, der Rest wird ignoriert
  if (voiceActive) {
    if (k == K_AC) voiceCancel();
    else if (k == K_EXE || k == K_ALPHA) voiceFinish();
    return;
  }

  // Buchstaben per Mehrfachtippen, SHIFT davor = Grossbuchstabe
  if (alpha[idx(mode)]) {
    const char* text;
    bool replace;
    if (multitap.feed(k, wasShift, millis(), text, replace)) {
      if (replace) s.inputBackspace();
      s.inputAppend(text);
      return;
    }
  } else {
    multitap.reset();
  }

  switch (k) {
    case K_SHIFT: shift = !wasShift; break;
    case K_ALPHA:
      if (wasShift) voiceStart();  // SHIFT+ALPHA: Spracheingabe
      else alpha[idx(mode)] = !alpha[idx(mode)];
      break;
    case K_MODE:
      if (wasShift && mode == Mode::Calc) degrees = !degrees;  // SHIFT+MODE: DEG/RAD
      else nextMode();
      break;
    case K_EXE: submit(); break;
    case K_DEL: s.inputBackspace(); break;
    case K_AC:
      if (wasShift) {
        requestOff();  // SHIFT+AC = aus, wie beim Casio
      } else if (s.input()[0]) {
        s.inputClear();
      } else if (mode == Mode::Terminal && net::sendNew()) {
        s.print("-- neue Sitzung --");
      }
      break;
    case K_UP: s.scroll(wasShift ? Screen::VIEW_ROWS : 1); break;
    case K_DOWN: s.scroll(wasShift ? -Screen::VIEW_ROWS : -1); break;
    case K_RIGHT:
      // Mehrfachtippen sofort abschliessen, z.B. fuer zwei Buchstaben auf derselben Taste
      break;
    default:
      if (const char* text = keyText(k, wasShift)) s.inputAppend(text);
      break;
  }
}

const char* mcpPinName(uint8_t pin) {
  static char buf[8];
  snprintf(buf, sizeof(buf), "GP%c%u", pin < 8 ? 'A' : 'B', pin % 8);
  return buf;
}

void pollKeys() {
  KeyEvent ev;
  while (keypad::poll(ev)) {
    Key k = keymapLookup(ev.row, ev.col);
    if (k == K_NONE || logKeys) {
      // Hilfe beim Ausmessen der Matrix: Position und MCP-Pins melden.
      Serial.printf("[key] %s Zeile %u (%s) ", ev.pressed ? "gedrueckt " : "losgelassen",
                    ev.row, mcpPinName(KEY_ROW_PINS[ev.row]));
      Serial.printf("Spalte %u (%s) -> %s\n", ev.col, mcpPinName(KEY_COL_PINS[ev.col]),
                    k == K_NONE ? "unbelegt" : keyName(k));
    }
    if (ignoreKeys) continue;
    lastActivity = millis();
    if (ev.pressed && k != K_NONE) onKey(k);
  }
  if (ignoreKeys && !keypad::active()) ignoreKeys = false;  // Wecktaste losgelassen
}

// ---------------------------------------------------------------------------
// Serieller Monitor: Zeile = Eingabe fuer den aktuellen Modus, ":..." = Befehl
// ---------------------------------------------------------------------------

Key keyByName(const char* name) {
  for (uint8_t k = 1; k < K_COUNT; k++) {
    if (!strcasecmp(name, keyName(static_cast<Key>(k)))) return static_cast<Key>(k);
  }
  return K_NONE;
}

void serialCommand(const char* cmd) {
  if (!strcmp(cmd, "calc")) setMode(Mode::Calc);
  else if (!strcmp(cmd, "term")) setMode(Mode::Terminal);
  else if (!strcmp(cmd, "cam")) setMode(Mode::Camera);
  else if (!strcmp(cmd, "keys")) {
    logKeys = !logKeys;
    Serial.printf("[app] Tastenprotokoll %s\n", logKeys ? "an" : "aus");
  } else if (!strcmp(cmd, "wifi")) net::enable(!net::enabled());
  else if (!strcmp(cmd, "new")) onKey(K_AC);
  else if (!strcmp(cmd, "ping")) {
    replyTo = mode;
    if (!net::sendPing()) Serial.println("[app] Bridge nicht verbunden");
  } else if (!strcmp(cmd, "off")) {
    requestOff();
  } else if (!strcmp(cmd, "rec")) {
    if (voiceActive) voiceFinish();
    else voiceStart();
  } else if (!strncmp(cmd, "key ", 4)) {
    Key k = keyByName(cmd + 4);
    if (k == K_NONE) Serial.println("[app] unbekannte Taste (Namen wie EXE, AC, SHIFT, 7, sin)");
    else onKey(k);
  } else {
    Serial.println("Befehle: :calc :term :cam  :keys (Tastenprotokoll)  :wifi (an/aus)");
    Serial.println("         :new (neue Claude-Sitzung)  :ping  :key NAME (Taste druecken)");
    Serial.println("         :rec (Spracheingabe starten/abschicken)  :off (ausschalten)");
    Serial.println("Jede andere Zeile wird im aktuellen Modus eingegeben und abgeschickt.");
  }
}

void pollSerial() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c != '\n') {
      if (serialLen < sizeof(serialBuf) - 1) serialBuf[serialLen++] = c;
      continue;
    }
    serialBuf[serialLen] = '\0';
    serialLen = 0;
    lastActivity = millis();
    if (serialBuf[0] == ':') {
      serialCommand(serialBuf + 1);
    } else if (serialBuf[0]) {
      screen().inputClear();
      screen().inputAppend(serialBuf);
      submit();
    }
  }
}

}  // namespace

namespace app {

void onOta(const char* message) {
  screen().print(message);
  display::render(screen());  // sofort zeigen, das Update blockiert loop()
}

void begin() {
  power::begin();
  Serial.begin(115200);
  display::begin();
  bool woke = power::wokeByKey();
  if (!woke) delay(500);  // Zeit fuer den seriellen Monitor, nur beim Kaltstart
  Serial.println(woke ? "\nCasio-Deck wacht auf" : "\nCasio-Deck startet");

  keypadOk = keypad::begin();
  if (!keypadOk) Serial.printf("[key] MCP23017 an 0x%02X antwortet nicht\n", MCP_ADDR);
  ignoreKeys = woke;

  net::begin(onBridge);
  ota::begin(onOta);
  setMode(mode);  // nach dem Aufwachen im selben Modus weiter
  lastActivity = millis();
  if (!woke) {
    screen(Mode::Calc).print("Casio-Deck bereit. MODE wechselt Rechner/Terminal/Kamera.");
    screen(Mode::Calc).print("SHIFT+AC schaltet aus. Serieller Monitor: ':help'.");
  }
  if (power::updatePending()) {
    // Neue Firmware gilt erst als gut, wenn sie wieder Updates annehmen kann
    screen().print("Neue Firmware: wird bestaetigt, sobald das WLAN steht ...");
    net::enable(true);
  }
}

void loop() {
  pollKeys();
  pollSerial();
  net::loop();
  ota::loop();
  if (power::updatePending() && ota::ready()) {
    power::confirmUpdate();
    screen().print("Neue Firmware bestaetigt.");
  }
  if (busy && net::state() != net::State::Online) {
    busy = false;  // Antwort kommt nicht mehr
    screen(replyTo).print("! Verbindung zur Bridge verloren");
  }
  sendPending();
  if (voiceActive && !mic::recording()) voiceFinish();  // Puffer voll

  // WLAN im Rechnermodus nach einer Weile abschalten (Akku)
  if (mode == Mode::Calc && net::enabled() && !busy && !pending[0] && !ota::running() &&
      !power::updatePending() && millis() - calcSince > WIFI_IDLE_OFF_MS) {
    net::enable(false);
  }

  // Ausschalten: auf Wunsch (SHIFT+AC) oder nach AUTO_OFF_MS ohne Eingabe
  if (!offRequested && !busy && !voiceActive && !pending[0] &&
      millis() - lastActivity > AUTO_OFF_MS &&
      !cannotSleep()) {
    offRequested = true;
  }
  if (offRequested && !keypad::active()) sleepNow();

  updateStatus();
  display::render(screen());

  // Nicht dauerhaft mit 100 % CPU kreisen; waehrend eines Tastenscans kuerzer.
  delay(keypad::active() ? 1 : 5);
}

void injectKey(Key k) { onKey(k); }

}  // namespace app
