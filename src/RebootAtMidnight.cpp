#include "RebootAtMidnight.h"

#include "Log.h"
#define LOGMODULE   LOGMODULE_REBOOTATMIDNIGHT
#include "Network.h"
#include "Reboot.h"


// Info:
//     Till V1.46 this was done only on change of day and if uptime was > 12h
// Now:
//     Since V1.47 we do that always on a day change

void RebootAtMidnightClass::init()
{
    _enabled = false;
}

void RebootAtMidnightClass::config(void)
{
}
void RebootAtMidnightClass::enable(void)
{
    if (_enabled || Network.inAPMode()) {
        return;
    }
    _enabled = true;
    adjustTicker();
}
void RebootAtMidnightClass::disable(void)
{
    _enabled = false;
    _ticker.detach();
}

// Setz den _ticker so, dass er beim nächsten Tageswechsel aufgerufen wird.
void RebootAtMidnightClass::adjustTicker(void)
{
    if (!_enabled) {
        return; // Should never happen
    }

    time_t now = time(NULL);
    if (now < 1761951600ll) { // 1761951600 ist: 31.10.2025 23:00:00 UTC ==> 01.11.2025 00:00:00 CET
        // Siehe: https://www.gaijin.at/de/tools/time-converter
        // wir haben noch keine gültige Uhrzeit ... also probieren wir es in einer Minute wieder
        if (millis()/1000ul > 86400ul) {
            // Das Sytem läuft jetzt aber schon seit über 24 Stunden
            // Also auch in diesem Fall: rebooten!
            DOLOG_IP("Starting reboot due runtime > 1 day ...");
            Reboot.startReboot();
            return;
        }
        _ticker.attach_scheduled(60, std::bind(&RebootAtMidnightClass::adjustTicker, this));
        return;
    }

    // Die Sekunden bis zum nächsten Tageswechsel berechnen weil wir solange ja dann warten müssen
    struct tm today, tomorrow;
    localtime_r(&now, &today);

    time_t nextDay = now;
    nextDay -= today.tm_sec;  nextDay -= today.tm_min * 60; // Minuten und Sekunden auf 0 setzen
    do {
        nextDay += 23ll * 3600ll; // und jetzt immer nur 23 Stunden addieren (wegen Sommer-/Winterzeit)
        localtime_r(&nextDay, &tomorrow);
    } while (today.tm_mday == tomorrow.tm_mday); // bis wir einen neuen Tag erreicht haben
    nextDay -= tomorrow.tm_hour * 3600ll; // Und noch die Stunden vom nächsten Tag wieder abziehen

    // Damit wir nicht ganz genau um 00:00:00 rebooten, sondern den Tages bzw Monatswechsel
    // noch in die History-Files schreiben geben wir einfach 5 Sekunden dazu
    time_t nextDayPlus5Sec = nextDay + 5;

    _ticker.detach();
    _ticker.attach_scheduled(nextDayPlus5Sec - now, std::bind(&RebootAtMidnightClass::doReboot, this));
    LOG_IP("Scheduling reboot in %llu seconds ...", nextDay - now);
}

void RebootAtMidnightClass::doReboot() {
    // OK wir hatten einen Tageswechsel ... also rebooten
    DOLOG_IP("Starting scheduled reboot...");
    Reboot.startReboot();
}

RebootAtMidnightClass RebootAtMidnight;

/* vim:set ts=4 et: */
