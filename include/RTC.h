// Global settings / defines for the RTC Memory

#pragma once

/*
see ..../framework-arduinoespressif8266/cores/esp8266/Esp.cpp 142ff

User-Offset 0-31 wird auch vom eboot Befehl verwendet (Offset wird in Blöcken zu 4 Bytes angegeben)
Dh: Offset  0 = Byte offset   0
    Offset 32 = Byte offset 128

┌────────┬───────────────────────┬─────────────────────────┬──────────────────────┐
│ Blöcke │        Adresse        │         Bereich         │ rtcUserMemory-Offset │
├────────┼───────────────────────┼─────────────────────────┼──────────────────────┤
│ 0–63   │ 0x60001100–0x600011FF │ System (SDK)            │ –                    │
├────────┼───────────────────────┼─────────────────────────┼──────────────────────┤
│ 64–95  │ 0x60001200–0x6000127F │ eboot-Befehl (128 Byte) │ 0–31                 │
├────────┼───────────────────────┼─────────────────────────┼──────────────────────┤
│ 96–191 │ 0x60001280–0x600013FF │ frei                    │ 32–127               │
└────────┴───────────────────────┴─────────────────────────┴──────────────────────┘

- Failsafe (Offset 32).
- Exception-Datensatz (Offset 33–104)
- Frei sind die Offsets 105–127 (92 Byte).
*/

#define RTC_FAILSAFE_OFFSET     32
#define RTC_FAILSAFE_MAXSIZE    4

#define RTC_EXCEPTION_OFFSET    33
#define RTC_EXCEPTION_MAXSIZE   288


static_assert(RTC_FAILSAFE_OFFSET >= 32, "RTC offsets 0-31 are used by eboot");
static_assert(RTC_EXCEPTION_OFFSET >= 32, "RTC offsets 0-31 are used by eboot");
static_assert(RTC_FAILSAFE_MAXSIZE % 4 == 0);

static_assert(RTC_FAILSAFE_OFFSET + (RTC_FAILSAFE_MAXSIZE / 4) <= RTC_EXCEPTION_OFFSET, "RTC: Failsafe overlaps Exception");
static_assert(RTC_EXCEPTION_OFFSET + (RTC_EXCEPTION_MAXSIZE / 4) <= 128, "RTC: Exception exceeds maximum");
static_assert(RTC_EXCEPTION_MAXSIZE % 4 == 0);

// vim:set ts=4 sw=4 et:
