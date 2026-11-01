#include "MaxDemand.h"

#include "Log.h"
#define LOGMODULE   LOGMODULE_SYSTEM

#include <LittleFS.h>

// Format v2: Werte in W (v1 hatte Wh gespeichert)
#define MAXDEMAND_FILENAME  "/max_1_6_0_w"

// Toleranz für die Übernahme des Zählerstands nach der vollen Viertelstunde:
// Kommt innerhalb dieser Zeit kein gültiger Datensatz, ist die Viertelstunde nicht auswertbar
#define MAXDEMAND_SLOT_START_TOLERANCE_SEC  60

// Erlaubte Abweichung der tatsächlich vergangenen Zeit von 15 Minuten zwischen zwei Referenzwerten
#define MAXDEMAND_SLOT_DURATION_MS          (15u * 60u * 1000u)
#define MAXDEMAND_SLOT_DURATION_TOLERANCE_MS (90u * 1000u)


void MaxDemandClass::init()
{
    load();
}

void MaxDemandClass::load()
{
    File f = LittleFS.open(MAXDEMAND_FILENAME, "r");
    if (!f) {
        return;
    }
    // Format: "MONTHKEY MAX_CURRENT MAX_PREVIOUS"
    String s = f.readString();
    f.close();

    long monthKey;
    unsigned long cur, prev;
    if (sscanf(s.c_str(), "%ld %lu %lu", &monthKey, &cur, &prev) == 3) {
        _monthKey = monthKey;
        _maxCurrentMonth = cur;
        _maxPreviousMonth = prev;
        LOGF_IP("1.6.0 loaded: current=%" PRIu32 " W, previous=%" PRIu32 " W", _maxCurrentMonth, _maxPreviousMonth);
    } else {
        LOG_EP("1.6.0 file invalid");
    }
}

void MaxDemandClass::save()
{
    File f = LittleFS.open(MAXDEMAND_FILENAME, "w");
    if (f) {
        f.printf("%ld %lu %lu\n", static_cast<long>(_monthKey),
                 static_cast<unsigned long>(_maxCurrentMonth), static_cast<unsigned long>(_maxPreviousMonth));
        f.close();
    }
}

void MaxDemandClass::checkMonthChange(int32_t monthKey)
{
    if (monthKey == _monthKey) {
        return;
    }
    if (_monthKey >= 0 && monthKey == _monthKey + 1) {
        _maxPreviousMonth = _maxCurrentMonth;
    } else if (_monthKey >= 0) {
        // Mehr als ein Monat ohne Daten (oder Zeit zurückgesprungen): Vormonat unbekannt
        _maxPreviousMonth = 0;
    }
    _maxCurrentMonth = 0;
    _monthKey = monthKey;
    LOGF_IP("1.6.0 month changed: previous=%" PRIu32 " W", _maxPreviousMonth);
    save();
}

void MaxDemandClass::onNewValidData(const struct tm &localTime, uint32_t value1_8_0)
{
    const uint32_t msNow = millis();

    // Viertelstunde aus der lokalen Zeit des Zählers (dient nur zum Erkennen des Wechsels).
    // Ob die vorige Viertelstunde lückenlos ist, wird über die tatsächlich vergangene Zeit
    // (millis) geprüft - so werden auch die Viertelstunden bei der Sommer-/Winterzeitumstellung
    // ausgewertet (01:45 -> 03:00 bzw. 02:45 Sommerzeit -> 02:00 Winterzeit).
    const int32_t slot = (localTime.tm_mday * 24 + localTime.tm_hour) * 4 + localTime.tm_min / 15;
    const int32_t monthKey = (1900 + localTime.tm_year) * 12 + localTime.tm_mon;

    if (slot != _lastSlot) {
        // Erster Datensatz einer neuen Viertelstunde
        const unsigned secsInSlot = (localTime.tm_min % 15) * 60 + localTime.tm_sec;
        const bool atSlotStart = secsInSlot < MAXDEMAND_SLOT_START_TOLERANCE_SEC;
        const uint32_t elapsedMs = msNow - _refMs;

        if (atSlotStart && _refValid && value1_8_0 >= _refValue &&
            elapsedMs > MAXDEMAND_SLOT_DURATION_MS - MAXDEMAND_SLOT_DURATION_TOLERANCE_MS &&
            elapsedMs < MAXDEMAND_SLOT_DURATION_MS + MAXDEMAND_SLOT_DURATION_TOLERANCE_MS)
        {
            // Die abgelaufene Viertelstunde gehört noch zum bisherigen Monat
            // (daher Monatswechsel erst danach prüfen)
            uint32_t power = (value1_8_0 - _refValue) * 4; // Wh pro 1/4h -> mittlere Leistung in W
            if (power > _maxCurrentMonth && _monthKey >= 0) {
                _maxCurrentMonth = power;
                LOGF_IP("1.6.0 new maximum: %" PRIu32 " W", _maxCurrentMonth);
                save();
            }
        }

        _refValid = atSlotStart;
        _refMs = msNow;
        _refValue = value1_8_0;
        _lastSlot = slot;
    }

    checkMonthChange(monthKey);
}

MaxDemandClass MaxDemand;

/* vim:set ts=4 et: */
