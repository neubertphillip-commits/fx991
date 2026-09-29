#include "keypad.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "config.h"

namespace {

// MCP23017-Register mit IOCON.BANK = 0: A/B-Paare liegen direkt hintereinander,
// ein 16-Bit-Zugriff (A = Low-Byte, B = High-Byte) erfasst beide Ports.
constexpr uint8_t REG_IODIR = 0x00;
constexpr uint8_t REG_GPINTEN = 0x04;
constexpr uint8_t REG_INTCON = 0x08;
constexpr uint8_t REG_IOCON = 0x0A;
constexpr uint8_t REG_GPPU = 0x0C;
constexpr uint8_t REG_GPIO = 0x12;

constexpr uint8_t IOCON_MIRROR = 0x40;  // INTA meldet Port A und B
constexpr uint8_t IOCON_ODR = 0x04;     // INT als Open-Drain (Pull-up am ESP32)

constexpr uint16_t ONLY_OUTPUT_PINS = (1u << 7) | (1u << 15);  // GPA7, GPB7
static_assert(KEY_LINES == 16, "eine Leitung pro MCP-Pin");
static_assert((KEY_DRIVE_MASK | KEY_SENSE_MASK) == 0xFFFF, "jede Leitung ist Treiber oder Eingang");
static_assert((KEY_DRIVE_MASK & KEY_SENSE_MASK) == 0, "Leitung ist Treiber und Eingang");
static_assert((ONLY_OUTPUT_PINS & KEY_SENSE_MASK) == 0, "GPA7/GPB7 koennen nur Treiber sein");
static_assert(KEY_SENSE_MASK & (1u << KL_A), "A muss Eingang sein");

constexpr uint32_t IDLE_POLL_MS = 50;  // Rueckfallebene ohne INTA, und fuer . / x10^x
constexpr uint8_t QUEUE_LEN = 16;

bool present = false;
bool scanning = false;
uint32_t lastScan = 0;
uint32_t lastIdlePoll = 0;

// Verbindungen: Bit b in x[a] = Leitungen a und b verbunden (symmetrisch gespeichert).
uint16_t stable[KEY_LINES];
uint16_t last[KEY_LINES];
uint8_t sameCount = 0;

KeyEvent queue[QUEUE_LEN];
uint8_t qHead = 0;
uint8_t qCount = 0;

bool write8(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool write16(uint8_t reg, uint16_t v) {
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(reg);
  Wire.write(static_cast<uint8_t>(v & 0xFF));
  Wire.write(static_cast<uint8_t>(v >> 8));
  return Wire.endTransmission() == 0;
}

bool read16(uint8_t reg, uint16_t& v) {
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<int>(MCP_ADDR), 2) != 2) return false;
  uint8_t a = Wire.read();
  uint8_t b = Wire.read();
  v = static_cast<uint16_t>(a | (b << 8));
  return true;
}

void connect(uint16_t* m, uint8_t a, uint8_t b) {
  m[a] |= static_cast<uint16_t>(1u << b);
  m[b] |= static_cast<uint16_t>(1u << a);
}

// Ein kompletter Scan:
// 1. Jeder Treiber einzeln LOW, die anderen Treiber HIGH (Push-Pull), die Eingaenge
//    lesen. Zwischen zwei Treibern liegt keine Taste, es gibt also keinen Kurzschluss.
// 2. A als Ausgang LOW, alle anderen (ausser GPA7/GPB7) als Eingang mit Pull-up: findet
//    . (A-N) und x10^x (A-H). Die Treiber sind dabei Eingaenge, weil 0, Ans und EXE A mit
//    Treibern verbinden und sonst LOW gegen HIGH stuenden.
bool scanAll(uint16_t* out) {
  memset(out, 0, sizeof(uint16_t) * KEY_LINES);
  if (!write16(REG_IODIR, KEY_SENSE_MASK)) return false;
  for (uint8_t d = 0; d < KEY_LINES; d++) {
    if (!(KEY_DRIVE_MASK & (1u << d))) continue;
    uint16_t gpio;
    if (!write16(REG_GPIO, static_cast<uint16_t>(KEY_DRIVE_MASK & ~(1u << d)))) return false;
    if (!read16(REG_GPIO, gpio)) return false;
    uint16_t low = static_cast<uint16_t>(~gpio & KEY_SENSE_MASK);
    for (uint8_t j = 0; low; j++, low >>= 1) {
      if (low & 1) connect(out, d, j);
    }
  }

  uint16_t gpio;
  const uint16_t aOut = static_cast<uint16_t>(ONLY_OUTPUT_PINS | (1u << KL_A));
  bool ok = write16(REG_GPIO, static_cast<uint16_t>(0xFFFF & ~(1u << KL_A))) &&
            write16(REG_IODIR, static_cast<uint16_t>(~aOut)) && read16(REG_GPIO, gpio);
  if (ok) {
    uint16_t low = static_cast<uint16_t>(~gpio & ~aOut);
    for (uint8_t j = 0; low; j++, low >>= 1) {
      if (low & 1) connect(out, KL_A, j);
    }
  }
  return write16(REG_IODIR, KEY_SENSE_MASK) && ok;
}

// Ruhezustand: alle Treiber LOW; jede Taste zwischen Treiber und Eingang zieht den
// Eingang runter und loest INTA aus.
void enterIdle() {
  scanning = false;
  uint16_t dummy;
  write16(REG_GPIO, 0);
  write16(REG_IODIR, KEY_SENSE_MASK);
  read16(REG_GPIO, dummy);  // Lesen von GPIO quittiert den Interrupt
}

// Im Ruhezustand nachsehen, ob eine Taste gedrueckt ist, auch . und x10^x (A treiben).
bool idleCheck() {
  uint16_t gpio;
  if (!read16(REG_GPIO, gpio)) return false;
  if ((gpio & KEY_SENSE_MASK) != KEY_SENSE_MASK) return true;
  const uint16_t nh = (1u << KL_N) | (1u << KL_H);
  bool pressed = write16(REG_IODIR, static_cast<uint16_t>(KEY_SENSE_MASK & ~(1u << KL_A))) &&
                 read16(REG_GPIO, gpio) && (gpio & nh) != nh;
  write16(REG_IODIR, KEY_SENSE_MASK);
  read16(REG_GPIO, gpio);  // A wieder Eingang: den dadurch ausgeloesten Interrupt quittieren
  return pressed;
}

void push(uint8_t a, uint8_t b, bool pressed) {
  if (qCount == QUEUE_LEN) return;  // voll: Ereignis verwerfen
  queue[(qHead + qCount) % QUEUE_LEN] = {a, b, pressed};
  qCount++;
}

void service() {
  uint32_t now = millis();
  if (!scanning) {
    bool wake = digitalRead(PIN_MCP_INT) == LOW;
    if (!wake && now - lastIdlePoll >= IDLE_POLL_MS) {
      lastIdlePoll = now;
      wake = idleCheck();
    }
    if (!wake) return;
    scanning = true;
    sameCount = 0;
    lastScan = now - KEY_SCAN_MS;
  }
  if (now - lastScan < KEY_SCAN_MS) return;
  lastScan = now;

  uint16_t cur[KEY_LINES];
  if (!scanAll(cur)) return;  // I2C-Fehler: beim naechsten Intervall erneut

  if (memcmp(cur, last, sizeof(cur)) == 0) {
    if (sameCount < 255) sameCount++;
  } else {
    memcpy(last, cur, sizeof(cur));
    sameCount = 1;
  }
  if (sameCount < KEY_DEBOUNCE_SCANS) return;

  bool any = false;
  for (uint8_t a = 0; a < KEY_LINES; a++) {
    uint16_t diff = static_cast<uint16_t>(stable[a] ^ cur[a]);
    for (uint8_t b = a + 1; b < KEY_LINES; b++) {
      if (diff & (1u << b)) push(a, b, cur[a] & (1u << b));
    }
    stable[a] = cur[a];
    any |= cur[a] != 0;
  }
  if (!any) enterIdle();
}

}  // namespace

namespace keypad {

bool begin() {
  pinMode(PIN_MCP_INT, INPUT_PULLUP);
  // Wire schaltet die internen Pull-ups (~45 kOhm) an SDA/SCL ein.
  Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQ);

  Wire.beginTransmission(MCP_ADDR);
  present = Wire.endTransmission() == 0;
  if (!present) return false;

  write8(REG_IOCON, IOCON_MIRROR | IOCON_ODR);
  write16(REG_GPIO, 0);  // Treiber erst LOW setzen, dann auf Ausgang schalten
  write16(REG_IODIR, KEY_SENSE_MASK);
  write16(REG_GPPU, static_cast<uint16_t>(~ONLY_OUTPUT_PINS));  // Pull-ups ueberall, wo moeglich
  write16(REG_INTCON, 0);  // Interrupt bei jeder Aenderung
  write16(REG_GPINTEN, KEY_SENSE_MASK);

  memset(stable, 0, sizeof(stable));
  memset(last, 0, sizeof(last));
  enterIdle();
  return true;
}

bool poll(KeyEvent& ev) {
  if (present) service();
  if (qCount == 0) return false;
  ev = queue[qHead];
  qHead = (qHead + 1) % QUEUE_LEN;
  qCount--;
  return true;
}

bool active() { return scanning; }

bool armWake() {
  if (!present) return false;
  enterIdle();
  uint16_t gpio;
  return read16(REG_GPIO, gpio) && (gpio & KEY_SENSE_MASK) == KEY_SENSE_MASK;
}

}  // namespace keypad
