#pragma once

#include <Arduino.h>
#include <time.h>

// Ermittelt das Maximum der mittleren 1/4h Leistung (1.6.0) [W] des aktuellen Monats und des Vormonats
//
// Zu jeder vollen Viertelstunde wird der Zählerstand 1.8.0 übernommen (bzw. der erste
// Wert der danach empfangen wird). Die Differenz zum Zählerstand der vorhergehenden
// vollen Viertelstunde ergibt den 1/4h Verbrauch [Wh], mal 4 die mittlere Leistung [W].
// Ist diese höher als das bisherige Monatsmaximum, wird das neue Maximum im Flash gespeichert.
// Beim Monatswechsel wird das aktuelle Maximum zum Vormonat und das aktuelle Maximum genullt.

class MaxDemandClass {
public:
    void init();
    void onNewValidData(const struct tm &localTime, uint32_t value1_8_0);
    uint32_t getCurrentMonth() const { return _maxCurrentMonth; }
    uint32_t getPreviousMonth() const { return _maxPreviousMonth; }

private:
    void load();
    void save();
    void checkMonthChange(int32_t monthKey);

    uint32_t _maxCurrentMonth = 0;  // 1.6.0 aktuelles Monat [W]
    uint32_t _maxPreviousMonth = 0; // 1.6.0 Vormonat [W]
    int32_t _monthKey = -1;         // (Jahr*12 + Monat) zu dem _maxCurrentMonth gehört, -1 = unbekannt

    int32_t _lastSlot = -1;         // Viertelstunde (lokale Zeit) des zuletzt empfangenen Datensatzes
    bool _refValid = false;         // Zählerstand zum Beginn der Viertelstunde verfügbar?
    uint32_t _refMs = 0;            // millis() bei Übernahme des Referenz-Zählerstands
    uint32_t _refValue = 0;         // 1.8.0 zum Beginn der Viertelstunde
};

extern MaxDemandClass MaxDemand;

/* vim:set ts=4 et: */
