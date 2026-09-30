// WLAN-Zugangsdaten, Bridge-Adresse und OTA-Passwort aus secrets.h (nicht im Repo).
#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h fehlt: secrets.example.h nach secrets.h kopieren und anpassen"
#include "secrets.example.h"
#endif
