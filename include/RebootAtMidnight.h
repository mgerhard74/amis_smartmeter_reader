#pragma once

#include <Ticker.h>

class RebootAtMidnightClass {
public:
    void init(void);
    void config(void);
    void enable(void);
    void disable(void);
    void adjustTicker(void);

private:
    void doReboot(void);
    bool _enabled;
    Ticker _ticker;
};
extern RebootAtMidnightClass RebootAtMidnight;

/* vim:set ts=4 et: */
