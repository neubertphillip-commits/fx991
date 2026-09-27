// Vorlage: nach secrets.h kopieren und ausfuellen. secrets.h ist in .gitignore.
#pragma once

#define WIFI_SSID "HandyHotspot"
#define WIFI_PASS "geheim"

// Adresse der Bridge. Leer = das Handy, das den Hotspot macht (Gateway). Empfohlen:
// Android waehlt das Hotspot-Netz seit Version 11 bei jedem Einschalten neu, eine feste
// IP stimmt dann nicht mehr. Feste IP nur, wenn die Bridge auf einem anderen Geraet laeuft.
#define BRIDGE_HOST ""

// Passwort fuer Firmware-Updates per WLAN (OTA). Beim Hochladen mit angeben,
// siehe firmware/README.md. Weglassen = Updates ohne Passwort.
#define OTA_PASSWORD "bitte-aendern"
