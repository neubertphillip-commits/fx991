// Vorlage: nach secrets.h kopieren und ausfuellen. secrets.h ist in .gitignore.
#pragma once

#define WIFI_SSID "HandyHotspot"
#define WIFI_PASS "geheim"

// IP des Handys im eigenen Hotspot (Android meist 192.168.x.1, `ifconfig` in Termux)
#define BRIDGE_HOST "192.168.43.1"

// Passwort fuer Firmware-Updates per WLAN (OTA). Beim Hochladen mit angeben,
// siehe firmware/README.md. Weglassen = Updates ohne Passwort.
#define OTA_PASSWORD "bitte-aendern"
