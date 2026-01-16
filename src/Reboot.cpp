/*
    Try doing a clean reboot meaning give some things time to shutdown
*/
#include "Reboot.h"

#include "AmisReader.h"
#include "config.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_SYSTEM
#include "ModbusSmartmeterEmulation.h"
#include "Mqtt.h"
#include "RemoteOnOff.h"
#include "ThingSpeak.h"

#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Ticker.h>

extern Ticker secTicker;
extern int valid;

void RebootClass::init()
{
}
void RebootClass::startReboot()
{
    if (_state != 0) {
        return;
    }
    _state = 1;
}

bool RebootClass::startUpdateFirmware()
{
// Start a firmware update
// End or disable all things stressing the cpu
// That would be:
// AmisReader, WatchdogPing, MQTT, MDNS, ModbusSmartmeterEmulation, RemoteOnOff, RebootAtMidnight, Thingspeak
    if (_state != 0) {
        return false;
    }
    _state = -1;
    AmisReader.end();
    valid = 6;
    ModbusSmartmeterEmulation.disable();
    ThingSpeak.disable();
    Mqtt.stop();
    MDNS.end();
    return true;
}
void RebootClass::endUpdateFirmware()
{
// Firmware has been updatet -> Now end the Webserver, Websocket and reboot
    if (_state == -1) {
        _state = 1;
    }
}

bool RebootClass::startUpdateLittleFS()
{
// Start a updating LittleFS filesystem
// End or disable all things stressing the cpu
// That would be:
// AmisReader, WatchdogPing, MQTT, MDNS, ModbusSmartmeterEmulation, RemoteOnOff, RebootAtMidnight, Thingspeak, LittleFS
    if (_state != 0) {
        return false;
    }
    _state = -2;
    AmisReader.end();
    valid = 6;
    ModbusSmartmeterEmulation.disable();
    ThingSpeak.disable();
    Mqtt.stop();
    MDNS.end();
    LittleFS.end(); // we can also end the filesystem as it will be overwritten
    return true;
}
void RebootClass::endUpdateLittleFS()
{
// LittleFS has been updatet -> Now end the Webserver, Websocket and reboot
    if (_state == -2) {
        _state = 1;
    }
}


void RebootClass::loop()
{
    if (_state <= 0) {
        return;
    }
    switch(_state++) {
        case 1:
            AmisReader.end();
            break;
        case 2:
            RemoteOnOff.prepareReboot();
            break;
        case 3:
            ThingSpeak.disable();
            break;
        case 4:
            secTicker.detach();
            break;
        case 5:
            Mqtt.stop();
            MDNS.end();
            break;
        case 6:
            ModbusSmartmeterEmulation.disable();
            break;
        case 7:
            LOG_PRINTF_IP("System is going to reboot");
            break;
        case 8:
            delay(150);
            break;
        case 9:
            LittleFS.end();
            break;
        case 10:
            delay(150);
            break;
        case 11:
            //ESP.wdtDisable();           // bootet 2x ???
            ESP.restart();
            while (1) {
                delay(1);
            }
    }
}

RebootClass Reboot;

/* vim:set ts=4 et: */
