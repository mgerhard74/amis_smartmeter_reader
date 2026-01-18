#pragma once

#include <time.h>

// Aktuell reiner Platzhalter für eine Struktur mit den aktuellsten Zählerdaten

// Später soll hier die Verteilung der Zählerdaten an die einzelnen Module
// (MQTT, Modbus, Websocket, HistoryFiles, ...) passieren

typedef struct {
    time_t ts;                  // Timestamp des Zählers (Bereits umgewandelt in UTC)
    char timeCp48Hex[11];       // MBUS CP48 Zeit Zählers (wie vom Zähler bekommen) - als Hexstring (ohne 0x)
    struct tm time;             // Zeit des Zählers (so wie vom Zähler bekommen)
    uint32_t results_u32[8];    // Die Zählerwerte
    int32_t  results_i32[1];    // Zählerwerte Inkasso (ist lt. Def ein int32)
    int valid;                  //
} DatabrokerResult_t;

extern DatabrokerResult_t Databroker;

/* vim:set ts=4 et: */
