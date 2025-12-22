#pragma once

#include <stdint.h>

typedef enum {
    LED_PINMODE_NONE,           // No LED connect (No I/O calls)
    LED_PINMODE_NO_INVERT,      // LED is on if we set output to HIGH
    LED_PINMODE_INVERT          // LED is on if we set output to LOW
} LedPinMode;

class LedSingleClass {
public:
    LedSingleClass(uint8_t pin, LedPinMode pinmode=LED_PINMODE_NO_INVERT);

    void loop();
    void turnOn();
    void turnOff();
    void turnBlink(uint32_t offIntervalMs, uint32_t onIntervalMs);
    void invert();
    void setBrightness(uint8_t percent);

private:
    uint8_t _pin = 0;
    LedPinMode _pinmode = LED_PINMODE_NO_INVERT;
    uint8_t _valueOff, _valueOn; // Durch setBrightness() berechnete Werte für ein/aus
    uint32_t _blinkIntervalsMs[2];
    uint8_t _blinkIntervalsIdx = 0;
    uint32_t _lastBlinkChangeMillis;

    typedef enum {
        on,
        off,
        blinkOn,
        blinkOff,
    } ledState_t;
    ledState_t _state = ledState_t::off;
    void writePin(uint8_t value);
};

extern LedSingleClass LedBlue;

/* vim:set ts=4 et: */
