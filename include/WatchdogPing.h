#pragma once

#include <AsyncPing.h>
#include <WString.h>

class WatchdogPingClass {
public:
    void init();
    void config(const char *host, unsigned int checkIntervalSec, unsigned int failCount);
    void loop();
    unsigned int restartAfterFailed;
    uint32_t checkIntervalMs;
    void enable();
    void disable();

private:
    AsyncPing _ping; // non-blocking ping
    bool onPingEndOfPing(const AsyncPingResponse& response);
    void startSinglePing();
    void stopSinglePing();
    uint32_t _lastPingStartedMs;
    unsigned int _counterFailed;
    bool _isWaitingForPingResult = false;
    bool _isEnabled = false;
    String _host;
};

extern WatchdogPingClass WatchdogPing;

/* vim:set ts=4 et: */
