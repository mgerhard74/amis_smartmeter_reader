#pragma once

#include <Ticker.h>

class RebootAtMidnightClass {
public:
    void init(void);
    void config(void);
    void enable(void);
    void disable(void);
private:
    void doReboot(void);
    void adjustTicker(void);
    bool _enabled = false;
    Ticker _ticker;
};
extern RebootAtMidnightClass RebootAtMidnight;

/* vim:set ts=4 et: */
