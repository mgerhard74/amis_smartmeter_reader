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

void LedSingleClass::loop() {
    /* handles a blinking LED */
    if (_state != LedState_t::BlinkOn && _state != LedState_t::BlinkOff) {
        return;
    }

    unsigned long now = millis();
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
    _state = LedState_t::On;
}

void LedSingleClass::turnOff() {
    writePin(_valueOff);
    _state = LedState_t::Off;
}

void LedSingleClass::turnBlink(unsigned long offIntervalMs, unsigned long onIntervalMs) {
    _blinkIntervalsMs[0] = offIntervalMs;
    _blinkIntervalsMs[1] = onIntervalMs;
    if (_state == LedState_t::Off) {
        _state = LedState_t::BlinkOff;
        _blinkIntervalsIdx = 0;
    } if (_state == LedState_t::On) {
        _state = LedState_t::BlinkOn;
        _blinkIntervalsIdx = 1;
    }
    _lastBlinkChangeMillis = millis();
    invert();
}

void LedSingleClass::invert() {
    if (_state == LedState_t::Off) {
        writePin(_valueOn);
        _state = LedState_t::On;
    } else if (_state == LedState_t::On) {
        writePin(_valueOff);
        _state = LedState_t::Off;
    } else if (_state == LedState_t::BlinkOff) {
        writePin(_valueOn);
        _state = LedState_t::BlinkOn;
    } else if (_state == LedState_t::BlinkOn) {
        writePin(_valueOff);
        _state = LedState_t::BlinkOff;
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


#ifdef LEDPIN
    // Serial1.txd: reroute pin function
    LedSingleClass LedBlue(LEDPIN, LED_PINMODE_NO_INVERT);
#else
    LedSingleClass LedBlue(0, LED_PINMODE_NONE);
#endif

/* vim:set ts=4 et: */
