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

constexpr uint16_t maskOf(const uint8_t* pins, uint8_t n) {
  return n == 0 ? 0 : static_cast<uint16_t>((1u << pins[n - 1]) | maskOf(pins, n - 1));
}
constexpr uint16_t ROW_MASK = maskOf(KEY_ROW_PINS, KEY_ROWS);
constexpr uint16_t COL_MASK = maskOf(KEY_COL_PINS, KEY_COLS);
static_assert(KEY_COLS <= 16, "zu viele Spalten");
static_assert((ROW_MASK & COL_MASK) == 0, "Pin ist gleichzeitig Zeile und Spalte");
static_assert((COL_MASK & ONLY_OUTPUT_PINS) == 0, "GPA7/GPB7 koennen nur Zeilen (Ausgaenge) sein");

constexpr uint32_t IDLE_POLL_MS = 50;  // Rueckfallebene, falls INTA nicht verdrahtet ist
constexpr uint8_t QUEUE_LEN = 16;

bool present = false;
bool scanning = false;
uint32_t lastScan = 0;
uint32_t lastIdlePoll = 0;

// Je Zeile ein Bit pro Spalte (Bit c = Spalte c gedrueckt).
uint16_t stable[KEY_ROWS];
uint16_t last[KEY_ROWS];
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

// Spalteneingaenge (low-aktiv) -> Bitmaske nach logischer Spaltennummer.
uint16_t colBits(uint16_t gpio) {
  uint16_t bits = 0;
  for (uint8_t c = 0; c < KEY_COLS; c++) {
    if (!(gpio & (1u << KEY_COL_PINS[c]))) bits |= 1u << c;
  }
  return bits;
}

// Zeile fuer Zeile auf LOW ziehen, die anderen bleiben HIGH, und Spalten lesen.
// Die Zeilen sind Push-Pull (GPA7/GPB7 duerfen keine Eingaenge sein). Bei mehreren
// gedrueckten Tasten in einer Spalte liegen zwei Ausgaenge ueber die Kohlekontakte
// gegeneinander; deren Widerstand begrenzt den Strom. Notfalls Dioden nachruesten.
bool scanMatrix(uint16_t* out) {
  for (uint8_t r = 0; r < KEY_ROWS; r++) {
    uint16_t gpio;
    if (!write16(REG_GPIO, ROW_MASK & ~(1u << KEY_ROW_PINS[r]))) return false;
    if (!read16(REG_GPIO, gpio)) return false;
    out[r] = colBits(gpio);
  }
  return true;
}

// Alle Zeilen LOW: jede Taste zieht dann ihre Spalte runter und loest INTA aus.
void enterIdle() {
  scanning = false;
  uint16_t dummy;
  write16(REG_GPIO, 0);
  read16(REG_GPIO, dummy);  // Lesen von GPIO quittiert den Interrupt
}

void push(uint8_t row, uint8_t col, bool pressed) {
  if (qCount == QUEUE_LEN) return;  // voll: Ereignis verwerfen
  queue[(qHead + qCount) % QUEUE_LEN] = {row, col, pressed};
  qCount++;
}

void service() {
  uint32_t now = millis();
  if (!scanning) {
    bool wake = digitalRead(PIN_MCP_INT) == LOW;
    if (!wake && now - lastIdlePoll >= IDLE_POLL_MS) {
      lastIdlePoll = now;
      uint16_t gpio;
      wake = read16(REG_GPIO, gpio) && (gpio & COL_MASK) != COL_MASK;
    }
    if (!wake) return;
    scanning = true;
    sameCount = 0;
    lastScan = now - KEY_SCAN_MS;
  }
  if (now - lastScan < KEY_SCAN_MS) return;
  lastScan = now;

  uint16_t cur[KEY_ROWS];
  if (!scanMatrix(cur)) return;  // I2C-Fehler: beim naechsten Intervall erneut

  if (memcmp(cur, last, sizeof(cur)) == 0) {
    if (sameCount < 255) sameCount++;
  } else {
    memcpy(last, cur, sizeof(cur));
    sameCount = 1;
  }
  if (sameCount < KEY_DEBOUNCE_SCANS) return;

  bool any = false;
  for (uint8_t r = 0; r < KEY_ROWS; r++) {
    uint16_t diff = stable[r] ^ cur[r];
    for (uint8_t c = 0; diff && c < KEY_COLS; c++) {
      if (diff & (1u << c)) push(r, c, cur[r] & (1u << c));
    }
    stable[r] = cur[r];
    any |= cur[r] != 0;
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

  // GPA7/GPB7 immer als Ausgang, auch wenn sie (noch) keine Zeile sind.
  const uint16_t outputs = ROW_MASK | ONLY_OUTPUT_PINS;
  write8(REG_IOCON, IOCON_MIRROR | IOCON_ODR);
  write16(REG_GPIO, ROW_MASK);  // Zeilen erst HIGH setzen, dann auf Ausgang schalten
  write16(REG_IODIR, static_cast<uint16_t>(~outputs));
  write16(REG_GPPU, static_cast<uint16_t>(~outputs));  // Spalten + unbenutzte Eingaenge
  write16(REG_INTCON, 0);  // Interrupt bei jeder Aenderung
  write16(REG_GPINTEN, COL_MASK);

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
  return read16(REG_GPIO, gpio) && (gpio & COL_MASK) == COL_MASK;
}

}  // namespace keypad
