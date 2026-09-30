#include "app.h"

#include <Arduino.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include <vector>

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
#include "store.h"
#include "viewer.h"

namespace {

// Rechner: offline, WLAN aus. Terminal: Prompts an die Bridge (claude -p).
// Kamera: Foto (+ optional Frage) an die Bridge.
// Spracheingabe (Terminal/Kamera): SHIFT+ALPHA startet, EXE oder SHIFT+ALPHA schickt,
// AC verwirft; die Bridge schickt den erkannten Text zurueck in die Eingabezeile.
// Dateien: Texte und Bilder vom Handy (Ordner der Bridge) offline lesen.
// MODE wechselt reihum; auf dem seriellen Monitor auch :calc, :term, :cam, :files.
// SHIFT+AC schaltet aus (Tiefschlaf), eine beliebige Taste wieder ein.
enum class Mode : uint8_t { Calc, Terminal, Camera, Files };
constexpr uint8_t MODE_COUNT = 4;
const char* const MODE_NAMES[MODE_COUNT] = {"RECHNER", "TERMINAL", "KAMERA", "DATEIEN"};

// RTC_DATA_ATTR: bleibt im Tiefschlaf erhalten (nicht bei leerem Akku)
RTC_DATA_ATTR Mode mode = Mode::Calc;
Screen screens[MODE_COUNT];
// ALPHA-Zustand je Modus: im Terminal und bei der Kamera-Frage meist Text.
RTC_DATA_ATTR bool alpha[MODE_COUNT] = {false, true, true, false};

bool keypadOk = false;
bool logKeys = false;  // alle Tastenereignisse seriell ausgeben
bool shift = false;
MultiTap multitap;

// Rechner
RTC_DATA_ATTR double ans = 0;
RTC_DATA_ATTR bool degrees = true;

// Bridge
bool busy = false;                       // Anfrage laeuft
Mode replyTo = Mode::Terminal;           // wohin die Antwort geschrieben wird

// WLAN ist nur an, wenn es gebraucht wird. Anfragen landen im Postausgang, das WLAN
// geht an, und sobald die Bridge verbunden ist, wird gesendet.
enum class Out : uint8_t { None, Prompt, Image, Audio, NewSession, Sync };
struct Outbox {
  Out kind = Out::None;
  Mode replyTo = Mode::Terminal;
  char text[Screen::INPUT_BYTES] = "";  // Frage (auch zum Foto)
  std::vector<uint8_t> image;           // Kopie des JPEG; die Kamera darf inzwischen aus
  uint32_t since = 0;
} outbox;
uint32_t lastNetUse = 0;       // letzte Anfrage oder Antwort, fuer WIFI_LINGER_MS
uint32_t keepOnlineUntil = 0;  // WLAN bewusst an (Update-Bereitschaft)

// Spracheingabe
bool voiceActive = false;  // Aufnahme laeuft (aus Sicht der App)
bool voiceSend = false;    // erkannten Text nach "done" abschicken (VOICE_AUTO_SEND)

// Ein/Aus
uint32_t lastActivity = 0;  // letzte Eingabe, fuer AUTO_OFF_MS
bool offRequested = false;  // ausschalten, sobald alle Tasten losgelassen sind
bool ignoreKeys = false;    // Taste, die aus dem Tiefschlaf geweckt hat, nicht auswerten

char serialBuf[Screen::INPUT_BYTES];
size_t serialLen = 0;

// Datei-Viewer: Liste, Textansicht, Abgleich mit dem Ordner auf dem Handy
struct Files {
  std::vector<store::Entry> entries;
  uint32_t freeKb = 0;
  int sel = 0;           // 0 = "Mit Handy abgleichen", ab 1 die Dateien
  int top = 0;           // erste sichtbare Listenzeile
  bool viewing = false;  // eine Datei ist offen
  bool image = false;
  char name[store::NAME_LEN + 1] = "";
  TextLayout layout;
  size_t first = 0;      // erste sichtbare Textzeile
  char msg[Screen::LINE_BYTES] = "";
  bool dirty = true;     // Anzeige neu aufbauen
  bool syncing = false;  // Abgleich laeuft
  uint16_t got = 0;
  uint16_t deleted = 0;
  uint32_t fileSize = 0;  // gerade empfangene Datei
  uint32_t received = 0;
} files;

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
    case Mode::Files:
      if (files.viewing && !files.image) {
        size_t n = files.layout.lines();
        size_t last = files.first + Screen::VIEW_ROWS < n ? files.first + Screen::VIEW_ROWS : n;
        snprintf(buf, sizeof(buf), "%s | %u-%u/%u | %s", MODE_NAMES[idx(Mode::Files)],
                 static_cast<unsigned>(files.first + 1), static_cast<unsigned>(last),
                 static_cast<unsigned>(n), files.name);
      } else {
        snprintf(buf, sizeof(buf), "%s | %u Dateien, %u kB frei | WLAN %s%s",
                 MODE_NAMES[idx(Mode::Files)], static_cast<unsigned>(files.entries.size()),
                 static_cast<unsigned>(files.freeKb), netState, files.syncing ? " | Abgleich" : "");
      }
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
void filesRefresh();

void setMode(Mode m) {
  if (voiceActive) voiceCancel();
  if (mode == Mode::Camera && m != Mode::Camera) camera::end();
  mode = m;
  shift = false;
  multitap.reset();
  if (m == Mode::Camera && !camera::begin()) screen().print(camera::error());
  if (m == Mode::Files) filesRefresh();
  updateStatus();
}

void nextMode() { setMode(static_cast<Mode>((idx(mode) + 1) % MODE_COUNT)); }

// ---------------------------------------------------------------------------
// Bridge
// ---------------------------------------------------------------------------

void submit();
void filesOnBridge(const char* type, const char* text, uint32_t size);

void onBridge(const char* type, const char* text, uint32_t size) {
  Screen& s = screen(replyTo);
  lastActivity = lastNetUse = millis();
  if (replyTo == Mode::Files) {  // Antworten zum Datei-Abgleich
    filesOnBridge(type, text, size);
    return;
  }
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
  if (mode == Mode::Calc || mode == Mode::Files) {
    s.print("(Spracheingabe nur im Terminal- und Kameramodus)");
    return;
  }
  if (busy || outbox.kind != Out::None) {
    s.print("(warte noch auf die letzte Antwort)");
    return;
  }
  net::enable(true);  // schon mal verbinden, waehrend gesprochen wird
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

bool queue(Out kind, const char* text, const char* echo);

// Aufnahme beenden und als WAV an die Bridge schicken (die Aufnahme bleibt im
// Mikrofon-Puffer, bis sie gesendet ist)
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
  if (!queue(Out::Audio, "", nullptr)) mic::release();
}

// ---------------------------------------------------------------------------
// Dateien: Liste, Textansicht mit Umbruch, Abgleich mit dem Ordner auf dem Handy.
// Die Bridge schickt nur neue und geaenderte Dateien (Vergleich per CRC-32) und
// loescht, was im Ordner nicht mehr liegt.
// ---------------------------------------------------------------------------

void sendPending();

constexpr int LIST_ROWS = Screen::VIEW_ROWS - 3;  // darunter: Leerzeile, Meldung, Hilfe

void filesMsg(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void filesMsg(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(files.msg, sizeof(files.msg), fmt, ap);
  va_end(ap);
  files.dirty = true;
}

void filesRefresh() {
  files.entries = store::list();
  files.freeKb = store::freeBytes() / 1024;
  int count = static_cast<int>(files.entries.size()) + 1;
  if (files.sel >= count) files.sel = count - 1;
  files.dirty = true;
}

void formatSize(uint32_t bytes, char* out, size_t cap) {
  if (bytes < 1024) snprintf(out, cap, "%u B", static_cast<unsigned>(bytes));
  else snprintf(out, cap, "%u kB", static_cast<unsigned>((bytes + 1023) / 1024));
}

void filesRenderList(Screen& s) {
  s.clear();
  int count = static_cast<int>(files.entries.size()) + 1;
  if (files.sel < files.top) files.top = files.sel;
  if (files.sel >= files.top + LIST_ROWS) files.top = files.sel - LIST_ROWS + 1;
  for (int i = files.top; i < count && i < files.top + LIST_ROWS; i++) {
    char line[Screen::LINE_BYTES];
    const char* mark = i == files.sel ? ">" : " ";
    if (i == 0) {
      snprintf(line, sizeof(line), "%s [Mit Handy abgleichen]", mark);
    } else {
      const store::Entry& e = files.entries[i - 1];
      char size[16];
      formatSize(e.size, size, sizeof(size));
      snprintf(line, sizeof(line), "%s %-*s %9s", mark, static_cast<int>(store::NAME_LEN), e.name,
               size);
    }
    s.print(line);
  }
  if (files.entries.empty()) s.print("  (noch keine Dateien, EXE auf [Mit Handy abgleichen])");
  s.print("");
  s.print(files.msg);
  s.print("Hoch/Runter waehlen, EXE oeffnen, SHIFT+Hoch/Runter Seite");
}

// Sichtbare Seite der offenen Textdatei; jede Zeile wird einzeln gelesen.
void filesRenderText(Screen& s) {
  s.clear();
  FILE* f = store::open(files.name);
  if (!f) {
    s.print("! Datei nicht lesbar");
    return;
  }
  size_t n = files.layout.lines();
  for (size_t i = files.first; i < n && i < files.first + Screen::VIEW_ROWS; i++) {
    uint8_t raw[Screen::LINE_BYTES * 2];
    uint32_t len = files.layout.end(i) - files.layout.start(i);
    if (len > sizeof(raw)) len = sizeof(raw);
    size_t got = 0;
    if (fseek(f, static_cast<long>(files.layout.start(i)), SEEK_SET) == 0) got = fread(raw, 1, len, f);
    char line[Screen::LINE_BYTES];
    textLineClean(raw, got, line, sizeof(line));
    s.print(line);
  }
  fclose(f);
}

void filesRender() {
  files.dirty = false;
  Screen& s = screen(Mode::Files);
  if (!files.viewing) filesRenderList(s);
  else if (!files.image) filesRenderText(s);
  // Bild: Ansicht wurde beim Oeffnen aufgebaut
}

void filesOpenImage() {
  Screen& s = screen(Mode::Files);
  s.clear();
  FILE* f = store::open(files.name);
  std::vector<uint8_t> data;
  if (f) {
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len > 0) {
      data.resize(static_cast<size_t>(len));  // grosse Bloecke landen im PSRAM
      data.resize(fread(data.data(), 1, data.size(), f));
    }
    fclose(f);
  }
  if (data.empty()) {
    s.print("! Datei nicht lesbar");
    return;
  }
  if (display::showJpeg(data.data(), data.size())) return;
  char line[Screen::LINE_BYTES];
  uint16_t w = 0, h = 0;
  char size[16];
  formatSize(static_cast<uint32_t>(data.size()), size, sizeof(size));
  s.print(files.name);
  if (jpegSize(data.data(), data.size(), w, h)) {
    snprintf(line, sizeof(line), "Bild %u x %u Pixel, %s", w, h, size);
  } else {
    snprintf(line, sizeof(line), "kein lesbares JPEG (%s)", size);
  }
  s.print(line);
  s.print("");
  s.print("(Bilder zeigt erst der Display-Treiber, AC zurueck)");
}

void filesOpenText() {
  files.layout.reset();
  FILE* f = store::open(files.name);
  if (f) {
    uint8_t buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) files.layout.feed(buf, n);
    fclose(f);
  }
  files.layout.finish();
}

void startSync();

void filesOpen() {
  if (files.sel == 0) {
    startSync();
    return;
  }
  const store::Entry& e = files.entries[files.sel - 1];
  strcpy(files.name, e.name);
  files.viewing = true;
  files.first = 0;
  files.image = store::isImage(files.name);
  files.dirty = false;
  if (files.image) {
    filesOpenImage();
  } else {
    filesOpenText();
    files.dirty = true;
  }
}

// Tasten im Dateimodus; false = normal weiterverarbeiten (SHIFT, MODE, SHIFT+AC).
bool filesKey(Key k, bool wasShift) {
  if (k == K_SHIFT || k == K_MODE || (k == K_AC && wasShift)) return false;
  if (k == K_AC) {
    if (files.viewing) {
      files.viewing = false;
      files.layout.reset();  // Speicher der Zeilentabelle freigeben
      files.dirty = true;
    }
    return true;
  }
  if (files.viewing) {
    if (files.image) return true;  // nur AC fuehrt zurueck
    size_t n = files.layout.lines();
    size_t page = Screen::VIEW_ROWS - 1;  // eine Zeile Ueberlappung
    size_t maxFirst = n > Screen::VIEW_ROWS ? n - Screen::VIEW_ROWS : 0;
    size_t step = 0;
    bool down = false;
    switch (k) {
      case K_DOWN: step = wasShift ? page : 1; down = true; break;
      case K_UP: step = wasShift ? page : 1; break;
      case K_EXE: case K_RIGHT: step = page; down = true; break;
      case K_LEFT: step = page; break;
      default: return true;
    }
    if (down) files.first = files.first + step < maxFirst ? files.first + step : maxFirst;
    else files.first = files.first > step ? files.first - step : 0;
    files.dirty = true;
    return true;
  }
  int count = static_cast<int>(files.entries.size()) + 1;
  int step = wasShift ? LIST_ROWS : 1;
  switch (k) {
    case K_DOWN: files.sel = files.sel + step < count ? files.sel + step : count - 1; break;
    case K_UP: files.sel = files.sel > step ? files.sel - step : 0; break;
    case K_EXE: case K_RIGHT: filesOpen(); break;
    default: return true;
  }
  files.dirty = true;
  return true;
}

// Abgleich beendet (fertig, Fehler oder Verbindung weg)
void filesSyncEnd(const char* error) {
  files.syncing = false;
  busy = false;
  store::abortWrite();  // halb empfangene Datei verwerfen
  filesRefresh();
  if (error) filesMsg("%s", error);
}

void filesOnBridge(const char* type, const char* text, uint32_t size) {
  if (!strcmp(type, "busy")) {
    busy = true;
  } else if (!strcmp(type, "del")) {
    if (store::remove(text)) files.deleted++;
  } else if (!strcmp(type, "file")) {
    files.fileSize = size;
    files.received = 0;
    if (!store::beginWrite(text, size)) {
      filesMsg("! %s: kann nicht speichern", text);
    } else {
      char kb[16];
      formatSize(size, kb, sizeof(kb));
      filesMsg("Lade %s (%s) ...", text, kb);
      if (!store::writing()) files.got++;  // leere Datei, schon fertig
    }
  } else if (!strcmp(type, "line")) {
    Serial.printf("[files] %s\n", text);
    filesMsg("%s", text);  // die letzte Zeile ist die Zusammenfassung
  } else if (!strcmp(type, "err")) {
    filesMsg("! %s", text);
  } else if (!strcmp(type, "done")) {
    char keep[Screen::LINE_BYTES];
    strcpy(keep, files.msg);
    filesSyncEnd(nullptr);
    filesMsg("%s", keep);
  }
}

// Binaer-Frames der Bridge: Inhalt der zuletzt mit "file" angekuendigten Datei
void filesOnBinary(const uint8_t* data, size_t len) {
  if (!files.syncing || !store::writing()) return;
  lastActivity = lastNetUse = millis();
  if (!store::write(data, len)) {
    filesMsg("! Schreibfehler (Speicher voll?)");
    return;
  }
  files.received += static_cast<uint32_t>(len);
  if (!store::writing()) {
    files.got++;
  } else if (files.fileSize > 0) {
    filesMsg("Lade ... %u %%", static_cast<unsigned>(100ULL * files.received / files.fileSize));
  }
}

void startSync() {
  if (!store::ready()) {
    filesMsg("! Dateispeicher nicht bereit");
    return;
  }
  if (!queue(Out::Sync, "", nullptr)) {
    filesMsg("(warte noch auf die letzte Antwort)");
    return;
  }
  files.viewing = false;
  filesMsg("Abgleich: verbinde mit dem Handy ...");
  sendPending();
}

// Liste mit CRC-32 an die Bridge; sie antwortet mit "del", "file" + Daten, "done".
bool sendSyncRequest() {
  filesRefresh();
  std::vector<net::FileInfo> info;
  info.reserve(files.entries.size());
  for (const store::Entry& e : files.entries) info.push_back({e.name, e.size, store::crc(e.name)});
  files.got = files.deleted = 0;
  files.syncing = true;
  filesMsg("Abgleich laeuft ...");
  if (net::sendSync(info.data(), info.size(), store::freeBytes())) return true;
  files.syncing = false;
  return false;
}

// ---------------------------------------------------------------------------
// Postausgang: senden, sobald die Bridge verbunden ist
// ---------------------------------------------------------------------------

void clearOutbox() {
  if (outbox.kind == Out::Audio) mic::release();
  outbox.image.clear();
  outbox.image.shrink_to_fit();
  outbox.kind = Out::None;
}

// Legt eine Anfrage in den Postausgang und schaltet das WLAN ein. `echo` wird vorher
// als eigene Zeile angezeigt (z.B. "> Frage").
bool queue(Out kind, const char* text, const char* echo) {
  if (busy || outbox.kind != Out::None) {
    screen().print("(warte noch auf die letzte Antwort)");
    return false;
  }
  outbox.kind = kind;
  outbox.replyTo = mode;
  strncpy(outbox.text, text, sizeof(outbox.text) - 1);
  outbox.text[sizeof(outbox.text) - 1] = '\0';
  outbox.since = lastNetUse = millis();
  if (echo) screen().print(echo);
  net::enable(true);
  if (net::state() != net::State::Online) screen().print("(verbinde mit der Bridge ...)");
  return true;
}

void sendPending() {
  if (outbox.kind == Out::None) return;
  if (net::state() != net::State::Online) {
    if (millis() - outbox.since > NET_GIVEUP_MS) {
      if (outbox.replyTo == Mode::Files) filesMsg("! Bridge nicht erreichbar");
      else screen(outbox.replyTo).print("! Bridge nicht erreichbar, Anfrage verworfen");
      clearOutbox();
    }
    return;
  }
  bool ok = false;
  switch (outbox.kind) {
    case Out::Prompt: ok = net::sendPrompt(outbox.text); break;
    case Out::Image:
      ok = net::sendBinary(outbox.image.data(), outbox.image.size()) && net::sendPrompt(outbox.text);
      break;
    case Out::Audio: {
      const uint8_t* wav;
      size_t len;
      ok = mic::wav(wav, len) && net::sendBinary(wav, len);
      break;
    }
    case Out::NewSession: ok = net::sendNew(); break;
    case Out::Sync: ok = sendSyncRequest(); break;
    case Out::None: break;
  }
  replyTo = outbox.replyTo;
  clearOutbox();
  lastNetUse = millis();
  if (ok) busy = true;
  else if (replyTo == Mode::Files) filesMsg("! Senden fehlgeschlagen");
  else screen(replyTo).print("! Senden fehlgeschlagen");
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
  char line[Screen::INPUT_BYTES + 2];
  snprintf(line, sizeof(line), "> %s", s.input());
  if (!queue(Out::Prompt, s.input(), line)) return;
  s.inputClear();
  sendPending();
}

// Foto aufnehmen und mit der Eingabe als Frage schicken (leer = "beschreibe das Bild").
void submitCamera() {
  Screen& s = screen(Mode::Camera);
  if (busy || outbox.kind != Out::None) {
    s.print("(warte noch auf die letzte Antwort)");
    return;
  }
  const uint8_t* jpeg;
  size_t len;
  if (!camera::capture(jpeg, len)) {  // sofort aufnehmen, gesendet wird nach dem Verbinden
    s.print(camera::error());
    return;
  }
  char line[Screen::INPUT_BYTES + 32];
  snprintf(line, sizeof(line), "> [Foto %u kB] %s", static_cast<unsigned>((len + 512) / 1024),
           s.input());
  if (queue(Out::Image, s.input(), line)) {
    outbox.image.assign(jpeg, jpeg + len);  // grosse Bloecke landen im PSRAM
    s.inputClear();
  }
  camera::release();
  sendPending();
}

void submit() {
  multitap.reset();
  switch (mode) {
    case Mode::Calc: submitCalc(); break;
    case Mode::Terminal: submitTerminal(); break;
    case Mode::Camera: submitCamera(); break;
    case Mode::Files:
      screen().inputClear();
      filesKey(K_EXE, false);
      break;
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
  clearOutbox();
  if (files.syncing) filesSyncEnd(nullptr);
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

// WLAN fuer OTA_WINDOW_MS anlassen, damit ein Update per WLAN ankommen kann.
void stayOnline() {
  keepOnlineUntil = millis() + OTA_WINDOW_MS;
  lastNetUse = millis();
  net::enable(true);
  char buf[80];
  snprintf(buf, sizeof(buf), "WLAN bleibt %u min an (Update: %s.local)",
           static_cast<unsigned>(OTA_WINDOW_MS / 60000), OTA_HOSTNAME);
  screen().print(buf);
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
    case K_NEG: return "-";
    case K_SQR: return "^2";
    case K_INV: return "^(-1)";
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

  if (mode == Mode::Files && filesKey(k, wasShift)) return;

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
      if (wasShift && mode == Mode::Calc) {
        degrees = !degrees;  // SHIFT+MODE im Rechner: DEG/RAD
      } else if (wasShift) {
        stayOnline();  // SHIFT+MODE im Terminal/Kamera: bereit fuer ein Update per WLAN
      } else {
        nextMode();
      }
      break;
    case K_EXE: submit(); break;
    case K_DEL: s.inputBackspace(); break;
    case K_AC:
      if (wasShift) {
        requestOff();  // SHIFT+AC = aus, wie beim Casio
      } else if (s.input()[0]) {
        s.inputClear();
      } else if (mode == Mode::Terminal && queue(Out::NewSession, "", "-- neue Sitzung --")) {
        sendPending();
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

void pollKeys() {
  KeyEvent ev;
  while (keypad::poll(ev)) {
    Key k = keymapLookup(ev.a, ev.b);
    if (k == K_NONE || logKeys) {
      // Hilfe beim Zuordnen: Kontaktnummer (hardware/tastatur_nummern.jpg), Leitungen, MCP-Pins
      Serial.printf("[key] %s Kontakt %u, Leitungen %s-%s (GP%c%u/GP%c%u) -> %s\n",
                    ev.pressed ? "gedrueckt  " : "losgelassen", keymapContact(ev.a, ev.b),
                    keyLineName(ev.a), keyLineName(ev.b), ev.a < 8 ? 'A' : 'B', ev.a % 8,
                    ev.b < 8 ? 'A' : 'B', ev.b % 8, k == K_NONE ? "unbelegt" : keyName(k));
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
  else if (!strcmp(cmd, "files")) setMode(Mode::Files);
  else if (!strcmp(cmd, "sync")) {
    if (mode != Mode::Files) setMode(Mode::Files);
    startSync();
  }
  else if (!strcmp(cmd, "keys")) {
    logKeys = !logKeys;
    Serial.printf("[app] Tastenprotokoll %s\n", logKeys ? "an" : "aus");
  } else if (!strcmp(cmd, "wifi") || !strcmp(cmd, "ota")) {
    stayOnline();
  } else if (!strcmp(cmd, "scan")) {
    net::scan();
  } else if (!strcmp(cmd, "new")) {
    onKey(K_AC);
  } else if (!strcmp(cmd, "ping")) {
    replyTo = mode;
    lastNetUse = millis();
    net::enable(true);
    if (!net::sendPing()) Serial.println("[app] Bridge (noch) nicht verbunden");
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
    Serial.println("Befehle: :calc :term :cam :files  :keys (Tastenprotokoll)  :ota (WLAN 5 min an)");
    Serial.println("         :new (neue Claude-Sitzung)  :ping  :key NAME (Taste druecken)");
    Serial.println("         :rec (Spracheingabe starten/abschicken)  :sync (Dateien abgleichen)");
    Serial.println("         :scan (WLAN-Netze anzeigen)  :off (ausschalten)");
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
  setCpuFrequencyMhz(CPU_MHZ);
  Serial.begin(115200);
  display::begin();
  bool woke = power::wokeByKey();
  if (!woke) delay(500);  // Zeit fuer den seriellen Monitor, nur beim Kaltstart
  Serial.println(woke ? "\nCasio-Deck wacht auf" : "\nCasio-Deck startet");

  keypadOk = keypad::begin();
  if (!keypadOk) Serial.printf("[key] MCP23017 an 0x%02X antwortet nicht\n", MCP_ADDR);
  ignoreKeys = woke;

  if (!store::begin()) Serial.println("[files] Dateispeicher nicht verfuegbar");
  net::begin(onBridge, filesOnBinary);
  ota::begin(onOta);
  setMode(mode);  // nach dem Aufwachen im selben Modus weiter
  lastActivity = millis();
  if (!woke) {
    screen(Mode::Calc).print("Casio-Deck bereit. MODE: Rechner/Terminal/Kamera/Dateien.");
    screen(Mode::Calc).print("SHIFT+AC schaltet aus. Serieller Monitor: ':help'.");
  }
  if (power::updatePending()) {
    // Neue Firmware gilt erst als gut, wenn sie wieder Updates annehmen kann
    screen().print("Neue Firmware: wird bestaetigt, sobald das WLAN steht ...");
    net::enable(true);
  }
}

// Leichtschlaf nur, wenn nichts laeuft, das der Schlaf stoeren oder verzoegern wuerde.
bool canNap() {
  uint32_t now = millis();
  return IDLE_LIGHT_SLEEP && keypadOk && !keypad::active() && !net::enabled() && !busy &&
         !voiceActive && outbox.kind == Out::None && !offRequested && !camera::isOn() &&
         !multitap.pending(now) && !ota::running() && now - lastActivity > IDLE_NAP_AFTER_MS;
}

void loop() {
  pollKeys();
  pollSerial();
  net::allowLowPower(busy && !files.syncing && !ota::running() && !power::updatePending() &&
                     static_cast<int32_t>(keepOnlineUntil - millis()) <= 0);
  net::loop();
  ota::loop();
  if (power::updatePending() && ota::ready()) {
    power::confirmUpdate();
    screen().print("Neue Firmware bestaetigt.");
  }
  if (busy && net::state() != net::State::Online) {
    busy = false;  // Antwort kommt nicht mehr
    if (replyTo == Mode::Files) filesSyncEnd("! Verbindung zur Bridge verloren");
    else screen(replyTo).print("! Verbindung zur Bridge verloren");
  }
  sendPending();
  if (voiceActive && !mic::recording()) voiceFinish();  // Puffer voll

  // WLAN nur solange es gebraucht wird (Akku): nach der letzten Antwort noch
  // WIFI_LINGER_MS fuer Nachzuegler der Bridge, dann aus
  bool netNeeded = busy || outbox.kind != Out::None || voiceActive || ota::running() ||
                   power::updatePending() || static_cast<int32_t>(keepOnlineUntil - millis()) > 0;
  if (net::enabled() && !netNeeded && millis() - lastNetUse > WIFI_LINGER_MS) {
    net::enable(false);
  }

  // Ausschalten: auf Wunsch (SHIFT+AC) oder nach AUTO_OFF_MS ohne Eingabe
  if (!offRequested && !busy && !voiceActive && outbox.kind == Out::None &&
      millis() - lastActivity > AUTO_OFF_MS &&
      !cannotSleep()) {
    offRequested = true;
  }
  if (offRequested && !keypad::active()) sleepNow();

  if (mode == Mode::Files && files.dirty) filesRender();
  updateStatus();
  display::render(screen());

  // Nichts zu tun: bis zur naechsten Taste leicht schlafen. Sonst nicht dauerhaft
  // mit 100 % CPU kreisen; waehrend eines Tastenscans kuerzer.
  if (!(canNap() && power::nap(IDLE_NAP_MAX_MS))) delay(keypad::active() ? 1 : 5);
}

void injectKey(Key k) { onKey(k); }

}  // namespace app
