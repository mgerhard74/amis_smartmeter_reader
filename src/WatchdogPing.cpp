/* Netzwerk Watchdog mittels Ping

   Es wird alle 'checkIntervalSec' Sekunden einen Ping absetzt.
   Sollten hintereinander 'failCount' davon fehlschlagen, wird
   das Reboot-Flag '*rebootFlag' gesetzt.
*/

#include "WatchdogPing.h"

#include "Log.h"
#define LOGMODULE   LOGMODULE_WATCHDOGPING
#include "Network.h"
#include "Reboot.h"

#include <cstdint>


// TODO(anyone) - reuse refactored classes for logging and debugging
#include "debug.h"
#include "config.h"

#if 0
// Doku/Beispiel zu AsyncPing
// https://github.com/akaJes/AsyncPing

/* callback for each answer/timeout of ping */
ping.on(true,[](const AsyncPingResponse& response){
    IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
    if (response.answer)
        Serial.printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%d ms\n", response.size, addr.toString().c_str(), response.icmp_seq, response.ttl, response.time);
    else
        Serial.printf("no answer yet for %s icmp_seq=%d\n", addr.toString().c_str(), response.icmp_seq);
    return false; //do not stop
});

/* callback for end of ping */
ping.on(false,[](const AsyncPingResponse& response){
  IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
  Serial.printf("total answer from %s sent %d recevied %d time %d ms\n",addr.toString().c_str(),response.total_sent,response.total_recv,response.total_time);
  if (response.mac)
    Serial.printf("detected eth address " MACSTR "\n",MAC2STR(response.mac->addr));
  return true; //doesn't matter
});
#endif


void WatchdogPingClass::init()
{
    _isWaitingForPingResult = false;
    _isEnabled = false;

    using std::placeholders::_1;
    _ping.on(false, std::bind(&WatchdogPingClass::onPingEndOfPing, this, _1));
}

void WatchdogPingClass::config(const IPAddress &targetIP, unsigned int checkIntervalSec, unsigned int failCount)
{
    bool wasWaitingForPingResult = _isWaitingForPingResult;

    stopSinglePing();

    _targetIP[0] = targetIP[0];
    _targetIP[1] = targetIP[1];
    _targetIP[2] = targetIP[2];
    _targetIP[3] = targetIP[3];

    _counterFailed = 0;
    checkIntervalMs = static_cast<uint32_t>(checkIntervalSec) * 1000;
    restartAfterFailed = failCount;

    if (_isEnabled) {
        if (wasWaitingForPingResult) {
            startSinglePing();
        } else {
            //_lastPingStartedMs = millis() - checkIntervalMs; // immediate start within next loop() call
            _lastPingStartedMs = millis();                     // start after waiting one interval
        }
    }
}

void WatchdogPingClass::enable()
{
    if (Network.inAPMode()) {
        return;
    }
    _isEnabled = true;
#if 0
    // This would start the ping immediately!
    // But if we are booting now, wifi needs first some time to connect.
    startSinglePing();
#else
    // Start watchdog with the next interval
    _lastPingStartedMs = millis();
#endif
}

void WatchdogPingClass::disable()
{
    _isEnabled = false;
    stopSinglePing();
}

bool WatchdogPingClass::onPingEndOfPing(const AsyncPingResponse& response)
{
    if (!_isEnabled) {
        return false;
    }
    if (!_isWaitingForPingResult) {
        // seems already canceled
        return false;
    }
    DBGOUT("Ping done, Result = " + String(response.answer) + ", RTT = " + String(response.total_time));
    if (response.answer) {
        if (_counterFailed > 0) {
            LOG_IP("Ping %u/%u to %s successful, RTT=%u", _counterFailed+1, restartAfterFailed, _targetIP.toString().c_str(), response.total_time);
        }
        _counterFailed = 0;
    } else {
        ++_counterFailed;
        LOG_WP("Ping %u/%u to %s failed!", _counterFailed, restartAfterFailed, _targetIP.toString().c_str());
        if (_counterFailed >= restartAfterFailed) {
            LOG_EP("Max ping failures reached, initiating reboot ...");
            Reboot.startReboot();
        }
    }
    _isWaitingForPingResult = false;
    return false; /* returning value does not matter in EndOfPing event */
}

void WatchdogPingClass::startSinglePing()
{
    if (_isWaitingForPingResult) {
        stopSinglePing();
    }
    // bool begin(const IPAddress &addr, u8_t count = 3, u32_t timeout = 1000);
    // bool begin(const char *host, u8_t count = 3, u32_t timeout = 1000);
    _lastPingStartedMs = millis();
    _ping.begin(_targetIP, 1, 1500); // single ping with timeout of 1500ms
    _isWaitingForPingResult = true;
}

void WatchdogPingClass::stopSinglePing()
{
    _isWaitingForPingResult = false;
    _ping.cancel();
}

void WatchdogPingClass::loop()
{
    if (!_isEnabled) {
        return;
    }
    if (_isWaitingForPingResult) {
        // We're still waiting on the ping result
        return;
    }
    uint32_t now = millis();
    if (now - _lastPingStartedMs < checkIntervalMs) {
        return;
    }
    startSinglePing();
}

WatchdogPingClass WatchdogPing;

/* vim:set ts=4 et: */
