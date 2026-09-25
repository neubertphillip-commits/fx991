// Casio-Deck PC-Simulator: laeuft mit der echten Firmware-Logik (app.cpp, net.cpp, ...)
// im Terminal und verbindet sich mit der Bridge.
//
//   ./casio-sim [--host 127.0.0.1] [--port 8765] [--cam foto.jpg]
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <string>

#include "../casio_deck/app.h"
#include "../casio_deck/net.h"
#include "Arduino.h"
#include "sim.h"

namespace {

termios savedTerm;
volatile sig_atomic_t quit = 0;
bool editing = false;
std::string editBuf;

void restoreTerminal() {
  tcsetattr(STDIN_FILENO, TCSANOW, &savedTerm);
  printf("\x1b[?25h\x1b[?1049l");  // Cursor an, Hauptbildschirm zurueck
  fflush(stdout);
}

void setupTerminal() {
  tcgetattr(STDIN_FILENO, &savedTerm);
  atexit(restoreTerminal);
  termios t = savedTerm;
  t.c_lflag &= ~(ICANON | ECHO);  // ISIG bleibt: Strg-C beendet
  t.c_cc[VMIN] = 0;
  t.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
  printf("\x1b[?1049h\x1b[?25l\x1b[2J");  // eigener Bildschirm, Cursor aus
  fflush(stdout);
}

int readByte(int timeoutMs) {
  pollfd p = {STDIN_FILENO, POLLIN, 0};
  if (poll(&p, 1, timeoutMs) <= 0) return -1;
  unsigned char c;
  return read(STDIN_FILENO, &c, 1) == 1 ? c : -1;
}

Key mapKey(int c) {
  switch (c) {
    case '0': return K_0;
    case '1': return K_1;
    case '2': return K_2;
    case '3': return K_3;
    case '4': return K_4;
    case '5': return K_5;
    case '6': return K_6;
    case '7': return K_7;
    case '8': return K_8;
    case '9': return K_9;
    case '.': case ',': return K_DOT;
    case '+': return K_ADD;
    case '-': return K_SUB;
    case '*': return K_MUL;
    case '/': return K_DIV;
    case '^': return K_POW;
    case '(': return K_LPAR;
    case ')': return K_RPAR;
    case '\r': case '\n': case '=': return K_EXE;
    case 127: case 8: return K_DEL;
    case '\t': return K_MODE;
    case 's': return K_SHIFT;
    case 'a': return K_ALPHA;
    case 'x': return K_EXP;
    case 'n': return K_ANS;
    case 'w': return K_SQRT;
    case 'i': return K_SIN;
    case 'o': return K_COS;
    case 't': return K_TAN;
    case 'l': return K_LN;
    case 'g': return K_LOG;
    default: return K_NONE;
  }
}

// Befehlszeile (Taste ':' oder '"'): geht wie eine Zeile vom seriellen Monitor an die App.
void editKey(int c) {
  if (c == '\r' || c == '\n') {
    Serial.feed((editBuf + "\n").c_str());
    editing = false;
  } else if (c == 127 || c == 8) {
    while (!editBuf.empty() && (editBuf.back() & 0xC0) == 0x80) editBuf.pop_back();
    if (!editBuf.empty()) editBuf.pop_back();
  } else if (c == 0x1b) {
    while (readByte(20) >= 0) {
    }  // Pfeiltasten o.ae. verwerfen
    editing = false;
  } else if (c >= 0x20) {
    editBuf += static_cast<char>(c);
  }
  sim::uiVersion++;
}

void pollKeyboard() {
  int c;
  while ((c = readByte(0)) >= 0) {
    if (editing) {
      editKey(c);
      continue;
    }
    if (c == ':' || c == '"') {
      editing = true;
      editBuf = c == ':' ? ":" : "";
      sim::uiVersion++;
      continue;
    }
    if (c == 0x1b) {
      int c2 = readByte(30);
      if (c2 == '[' || c2 == 'O') {
        int c3 = readByte(30);
        if (c3 == 'A') app::injectKey(K_UP);
        else if (c3 == 'B') app::injectKey(K_DOWN);
        else if (c3 == 'C') app::injectKey(K_RIGHT);
        else if (c3 == 'D') app::injectKey(K_LEFT);
        else if (c3 == '3' && readByte(30) == '~') app::injectKey(K_DEL);
      } else {
        app::injectKey(K_AC);
      }
      continue;
    }
    Key k = mapKey(c);
    if (k != K_NONE) app::injectKey(k);
  }
}

void onSignal(int) { quit = 1; }

void usage(const char* prog) {
  fprintf(stderr, "Aufruf: %s [--host HOST] [--port PORT] [--cam BILD.jpg]\n", prog);
  exit(2);
}

}  // namespace

const char* sim::lineEdit() { return editing ? editBuf.c_str() : nullptr; }

int main(int argc, char** argv) {
  const char* host = "127.0.0.1";
  int port = 8765;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--host") && i + 1 < argc) host = argv[++i];
    else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--cam") && i + 1 < argc) sim::cameraImage = argv[++i];
    else usage(argv[0]);
  }
  if (!isatty(STDIN_FILENO)) {
    fprintf(stderr, "Der Simulator braucht ein Terminal.\n");
    return 1;
  }

  setupTerminal();
  signal(SIGINT, onSignal);
  signal(SIGTERM, onSignal);
  signal(SIGPIPE, SIG_IGN);  // abgebrochene Verbindung nicht als Absturz

  net::setBridge(host, static_cast<uint16_t>(port));
  app::begin();
  while (!quit) {
    pollKeyboard();
    app::loop();
  }
  return 0;
}
