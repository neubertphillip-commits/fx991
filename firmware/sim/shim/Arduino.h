// Minimaler Arduino-Ersatz fuer den PC-Simulator.
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <deque>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

uint32_t millis();
void delay(uint32_t ms);
inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }

// Serieller Monitor: Ausgabe landet im Log unter dem simulierten Display,
// Eingaben kommen aus der Kommandozeile des Simulators (Taste ':').
class SimSerial {
 public:
  void begin(unsigned long) {}
  void setTxTimeoutMs(uint32_t) {}

  int available() { return static_cast<int>(in_.size()); }
  int read() {
    if (in_.empty()) return -1;
    char c = in_.front();
    in_.pop_front();
    return static_cast<unsigned char>(c);
  }
  void feed(const char* text) {
    while (*text) in_.push_back(*text++);
  }

  size_t print(const char* s) { return write(s, strlen(s)); }
  size_t println(const char* s = "") { return print(s) + print("\n"); }
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return n > 0 ? print(buf) : 0;
  }

 private:
  size_t write(const char* s, size_t len);
  std::deque<char> in_;
};

extern SimSerial Serial;
