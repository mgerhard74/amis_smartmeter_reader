#include "MaxDemand.h"

#include "Log.h"
#define LOGMODULE   LOGMODULE_SYSTEM

#include <LittleFS.h>

// Toleranz für die Übernahme des Zählerstands nach der vollen Viertelstunde:
// Kommt innerhalb dieser Zeit kein gültiger Datensatz, ist die Viertelstunde nicht auswertbar
#define MAXDEMAND_SLOT_START_TOLERANCE_SEC  30



void MaxDemandClass::init()
{
    loadMonthHistory();
    loadMonthCurrent();
}

#define PTR_TO_NEXT_NUM(PTR)        while(*(PTR) && (*(PTR)<'0' || *(PTR)>'9')) (PTR)++
#define PTR_TO_NEXT_NONNUM(PTR)     while(*(PTR)>='0' && *(PTR)<='9') (PTR)++

void MaxDemandClass::appendMonthHistory(uint32_t yymm, uint32_t v_1_6_0, uint32_t ts) {
    //     4  1   10  1  10   1 1   = 28
    //   "yymm NUMBER1 NUMBER2\n\0"
    File f = LittleFS.open(MAXDEMAND_HISTORY_FILENAME, "a");
    if (!f) {
        LOGF_EP("Error opening '%s'", MAXDEMAND_HISTORY_FILENAME);
        return;
    }
    f.printf("%04" PRIu32 " %" PRIu32 " %" PRIu32 "\n", yymm, v_1_6_0, ts);
    f.close();
}


static void splitDataLine(const String &s, uint32_t &yymm, uint32_t &value, uint32_t &ts) {
    yymm = 0;
    value = 0;
    ts = 0;

    const char *c = s.c_str();

    PTR_TO_NEXT_NUM(c);
    if (! *c) {
        return;
    }
    yymm = strtoul(c, nullptr, 10);
    PTR_TO_NEXT_NONNUM(c);
    if (! *c) {
        return;
    }

    PTR_TO_NEXT_NUM(c);
    if (! *c) {
        return;
    }
    value = strtoul(c, nullptr, 10);
    PTR_TO_NEXT_NONNUM(c);
    if (! *c) {
        return;
    }

    PTR_TO_NEXT_NUM(c);
    if (! *c) {
        return;
    }
    ts = strtoul(c, nullptr, 10);
}

void MaxDemandClass::loadMonthHistory()
{
    /*
    Im File "/maxdemand_hist" speichern wir Werte die erst am Monatsende geschrieben werden können
    Format:             YYMM max1.6.0 timestamp
    Bsp:            Im Monat 12 / 2025 sieht das wie folgt aus:
                        2509 4584 1757284200
                        2510 3953 1760442300
                        2511 4020 1764437400
    */

    File f;
    String buffer;

    _prevMonthYYMM = 0;
    _prevMonthMaxValue = 0;
    _prevMonthTS = 0;

    f = LittleFS.open(MAXDEMAND_HISTORY_FILENAME, "r");
    if (!f) {
        return;
    }
    // wir brauchen nur die letzte Zeile
    while (f.available()) {
        buffer = f.readStringUntil('\n');
    }
    f.close();

    uint32_t yymm, value, ts;
    splitDataLine(buffer, yymm, value, ts);
    if (yymm != 0 && value != 0 && ts != 0) {
        _prevMonthYYMM = yymm;
        _prevMonthMaxValue = value;
        _prevMonthTS = ts;
    }
}

void MaxDemandClass::loadMonthCurrent()
{
    File f = LittleFS.open(MAXDEMAND_CURRENT_FILENAME, "r");
    if (!f) {
        return;
    }

    // Format: - eine Zeile
    //  "MONTHKEY CURRENT_MONTH_MAX CURRENT_MONTH_MAX_TS\n"

    String buffer = f.readStringUntil('\n');
    uint32_t yymm, value, ts;
    splitDataLine(buffer, yymm, value, ts);
    if (yymm != 0 && value != 0 && ts != 0) {
        _currentMonthYYMM = yymm;
        _currentMonthMaxValue = value;
        _currentMonthTS = ts;
        LOGF_IP("1.6.0 loaded: current=%" PRIu32 " W, previous=%" PRIu32 " W", _currentMonthMaxValue, _prevMonthMaxValue);
    } else {
        LOG_EP("1.6.0 file invalid");
    }
}

void MaxDemandClass::saveMonthCurrent()
{
    File f = LittleFS.open(MAXDEMAND_CURRENT_FILENAME, "w");
    if (f) {
        f.printf("%" PRIu32 " %" PRIu32 " %" PRIu32 "\n", _currentMonthYYMM, _currentMonthMaxValue, _currentMonthTS);
        f.close();
    } else {
        LOGF_EP("Error opening '%s'", MAXDEMAND_CURRENT_FILENAME);
    }
}

#define QUARTER_HOUR_SECS (15u*60u)

void MaxDemandClass::checkNewMax(const struct tm &localTime, uint32_t ts, uint32_t value1_8_0)
{
    // struct tm *localTime; time_t rawtime = (time_t)ts;   localTime = localtime(&rawtime);

    // timestamp_slot: timestamp auf 15 Minuten abgerundet des gerade übermittelten Wertes
    const uint32_t ts_slot = (ts / QUARTER_HOUR_SECS) * QUARTER_HOUR_SECS;

    if (_quarterStartTS == ts_slot) {
        // Bin noch immer in der gleichen Viertelstunde
        return;
    }

    // Neuer Viertel Stunde hat begonnen !
    // _quarterStartTS steht noch auf dem Beginn der vorherigen Virtel-Stunde

    // YYMM des gerade übermittelten Wertes ermitteln (Achtung: tm_mon ist 0 basierend)
    const uint32_t yymm = (localTime.tm_year - 100) * 100 + localTime.tm_mon + 1;

    /*
    uint32_t prev_yymm = localTime.tm_year - 100;
    prev_yymm -= (localTime.tm_mon == 0) ?1 :0;
    prev_yymm *= 100;
    prev_yymm += (localTime.tm_mon == 0) ?12 :localTime.tm_mon;
    */


    if (_quarterStartTS + QUARTER_HOUR_SECS == ts_slot) {
        // es handelt sich genau um die nach _quarterStartTS beginnende Viertelstunde
        if (ts <= _quarterStartTS + QUARTER_HOUR_SECS + MAXDEMAND_SLOT_START_TOLERANCE_SEC && value1_8_0 >= _quarterStart_1_8_0) {
            // wir akzeptieren den Wert als gültig - liegt innerhalb der Toleranz von 30 (MAXDEMAND_SLOT_START_TOLERANCE_SEC) Sekunden
            const uint32_t v_1_6_0 = (value1_8_0 - _quarterStart_1_8_0) * 4; // mal 4, weil der Wert auf Stunden hochgerechnet wird
            if (v_1_6_0 > _currentMonthMaxValue) {
                // ein neuer "Monatshöchstwert"
                _currentMonthMaxValue = v_1_6_0;
                _currentMonthTS = _quarterStartTS;
                saveMonthCurrent();
            }
        }
    }

    _quarterStartTS = ts_slot;
    _quarterStart_1_8_0 = value1_8_0;
    _quarterStartYYMM = yymm;
}


void MaxDemandClass::checkNewMonth()
{
    // Hat sich das Monat geändert ?
    if (_quarterStartYYMM == _currentMonthYYMM) {
        return;
    }

    if (_currentMonthYYMM && _prevMonthYYMM < _currentMonthYYMM) {
        appendMonthHistory(_currentMonthYYMM, _currentMonthMaxValue, _currentMonthTS);
    }
    _prevMonthYYMM = _currentMonthYYMM;
    _prevMonthMaxValue = _currentMonthMaxValue;
    _prevMonthTS = _currentMonthTS;

    _currentMonthYYMM = _quarterStartYYMM;
    _currentMonthMaxValue = 0;
    _currentMonthTS = _quarterStartTS;
    saveMonthCurrent();
}

void MaxDemandClass::onNewValidData(const struct tm &localTime, uint32_t ts, uint32_t value1_8_0)
{
    checkNewMax(localTime, ts, value1_8_0);
    checkNewMonth();
}

MaxDemandClass MaxDemand;

/* vim:set ts=4 et: */
