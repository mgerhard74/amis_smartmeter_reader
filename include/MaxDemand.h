#pragma once

#include <Arduino.h>
#include <time.h>

// Aktuelles Monat
#define MAXDEMAND_CURRENT_FILENAME  "/maxdemand_curr"

// Historische Werte - Werden immer am Monatsende bzw eigentlich beim Beginn
// eines neuen Monats geschrieben

#define MAXDEMAND_HISTORY_FILENAME  "/maxdemand_hist"

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
    void onNewValidData(const struct tm &localTime, uint32_t ts, uint32_t value1_8_0);
    uint32_t getCurrentMonth() const { return _currentMonthMaxValue; }
    uint32_t getPreviousMonth() const { return _prevMonthMaxValue; }

private:
    void loadMonthCurrent();
    void saveMonthCurrent();
    void checkMonthChange(int32_t monthKey);
    void loadMonthHistory();
    void appendMonthHistory(uint32_t yymm, uint32_t v_1_6_0, uint32_t ts);
    void checkNewMax(const struct tm &localTime, uint32_t ts, uint32_t value1_8_0);
    void checkNewMonth();

    uint32_t _prevMonthYYMM;        // Vormonat [YYMM]
    uint32_t _prevMonthMaxValue;    // Max 1.6.0 Vormonat [W]
    uint32_t _prevMonthTS;          // Timestamp Vormonat - Beginn der Viertelstunde

    uint32_t _currentMonthYYMM;     // Aktuelles Monat [YYMM]
    uint32_t _currentMonthMaxValue; // Max 1.6.0 aktuelles Monat [W]
    uint32_t _currentMonthTS;       // Timestamp aktuelle Monat - Beginn der Viertelstunde

    uint32_t _quarterStartYYMM;     // YYMM zu dem die aktuelle Viertelstunde gehört
    uint32_t _quarterStart_1_8_0;   // 1.8.0 zum Beginn der aktuellen Viertelstunde
    uint32_t _quarterStartTS;       // Timestamp Beginn der aktuellen Viertelstunde
};

extern MaxDemandClass MaxDemand;

/* vim:set ts=4 et: */
