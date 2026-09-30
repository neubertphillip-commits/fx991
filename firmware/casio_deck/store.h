// Dateiablage fuer den Datei-Viewer: Texte und Bilder, die die Bridge vom Handy
// schickt (Abgleich mit einem Ordner dort). Auf dem ESP32 im LittleFS (Partition
// "spiffs", 1,5 MB), im PC-Simulator in einem Ordner.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <vector>

namespace store {

constexpr size_t NAME_LEN = 40;

struct Entry {
  char name[NAME_LEN + 1];
  uint32_t size;
};

// Nur PC-Simulator: Ordner statt Flash (vor begin() aufrufen).
void setRoot(const char* dir);

bool begin();  // Dateisystem einhaengen (beim ersten Mal formatieren)
bool ready();

// Nur Buchstaben, Ziffern, . _ - (die Bridge vergibt solche Namen), kein '.' am Anfang.
bool validName(const char* name);
bool isImage(const char* name);  // .jpg

std::vector<Entry> list();  // sortiert nach Name
FILE* open(const char* name);  // zum Lesen; mit fclose schliessen
bool remove(const char* name);
uint32_t crc(const char* name);  // CRC-32 des Inhalts, 0 bei Fehler
uint32_t freeBytes();

// Empfang einer Datei in Stuecken. Eine vorhandene Datei gleichen Namens wird sofort
// geloescht (Platz im Flash); die neue entsteht als Zwischendatei und bekommt erst
// nach dem letzten Stueck ihren Namen. Bricht der Empfang ab, fehlt die Datei bis
// zum naechsten Abgleich.
bool beginWrite(const char* name, uint32_t size);
// false bei Schreibfehler oder zu vielen Daten; nach dem letzten Byte fertig.
bool write(const uint8_t* data, size_t len);
bool writing();
void abortWrite();

}  // namespace store
