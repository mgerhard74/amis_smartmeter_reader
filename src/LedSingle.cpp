#include "ProjectConfiguration.h"

#include "LedSingle.h"

#include <Arduino.h>

LedSingleClass::LedSingleClass(uint8_t pin, LedPinMode pinmode) {
    _pin = pin;
    _pinmode = pinmode;
    if (_pinmode != LED_PINMODE_NONE) {
        pinMode(_pin, OUTPUT);
    }
    setBrightness(100);
    turnOff();
}

extern void writeEvent(String, String, String, String);

void LedSingleClass::loop() {
    /* handles a blinking LED */
    if (_state != blinkOn && _state != blinkOff) {
        return;
    }

    uint32_t now = millis();
    if (now - _lastBlinkChangeMillis < _blinkIntervalsMs[_blinkIntervalsIdx]) {
        return;
    }
    _lastBlinkChangeMillis = now;
    if(++_blinkIntervalsIdx >= std::size(_blinkIntervalsMs)) {
        _blinkIntervalsIdx = 0;
    }
    invert();
}

void LedSingleClass::turnOn() {
    writePin(_valueOn);
    _state = on;
}

void LedSingleClass::turnOff() {
    writePin(_valueOff);
    _state = off;
}

void LedSingleClass::turnBlink(uint32_t offIntervalMs, uint32_t onIntervalMs) {
    _blinkIntervalsMs[0] = offIntervalMs;
    _blinkIntervalsMs[1] = onIntervalMs;

    _lastBlinkChangeMillis = millis();
    // Do next blink step immediately next time loop() gets called
    _lastBlinkChangeMillis -= std::max(offIntervalMs, onIntervalMs);

    if (_state == off) {
        _blinkIntervalsIdx = 0;
        _state = blinkOff;
    } else if (_state == on) {
        _blinkIntervalsIdx = 1;
        _state = blinkOn;
    }
}

void LedSingleClass::invert() {
    if (_state == off) {
        writePin(_valueOn);
        _state = on;
    } else if (_state == on) {
        writePin(_valueOff);
        _state = off;
    } else if (_state == blinkOff) {
        writePin(_valueOn);
        _state = blinkOn;
    } else if (_state == blinkOn) {
        writePin(_valueOff);
        _state = blinkOff;
    }
}

void LedSingleClass::setBrightness(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }
    if (_pinmode == LED_PINMODE_NO_INVERT ) {
        _valueOff = 0;
        _valueOn = (255 * percent) / 100;
    } else {
        _valueOff = 255;
        _valueOn = 255 - ((255 * percent) / 100);
    }
}

void LedSingleClass::writePin(uint8_t value) {
    if (_pinmode != LED_PINMODE_NONE) {
        analogWrite(_pin, value);
        // digitalWrite(_pin, HIGH);
    }
}


#ifdef LED_PIN
    // LED via 470 to VCC --> 0 = leuchtet, 255 ist aus --> invertiert
    LedSingleClass LedBlue(LED_PIN, LED_PINMODE_INVERT);
#else
    LedSingleClass LedBlue(0, LED_PINMODE_NONE);
#endif

/* vim:set ts=4 et: */
