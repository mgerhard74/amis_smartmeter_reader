/*
    Try doing a clean reboot meaning give some things time to shutdown
*/
#include "Reboot.h"

#include "AmisReader.h"
#include "config.h"
#include "ModbusSmartmeterEmulation.h"
#include "RemoteOnOff.h"

#include <LittleFS.h>
#include <Ticker.h>

extern Ticker secTicker;
extern Ticker mqttTimer;
extern void writeEvent(String, String, String, String);
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

void RebootClass::startUpdateFirmware()
{
// Start a firmware update
// End or disable all things stressing the cpu
// That would be:
// AmisReader, WatchdogPing, MQTT, MDNS, ModbusSmartmeterEmulation, RemoteOnOff, RebootAtMidnight, Thingspeak
    AmisReader.end();
    valid = 6;
    ModbusSmartmeterEmulation.disable();
}
void RebootClass::endUpdateFirmware()
{
// Firmware has been updatet -> Now end the Webserver, Websocket and reboot
    startReboot();
}

void RebootClass::startUpdateLittleFS()
{
// Start a updating LittleFS filesystem
// End or disable all things stressing the cpu
// That would be:
// AmisReader, WatchdogPing, MQTT, MDNS, ModbusSmartmeterEmulation, RemoteOnOff, RebootAtMidnight, Thingspeak, LittleFS
    AmisReader.end();
    valid = 6;
    ModbusSmartmeterEmulation.disable();
    LittleFS.end(); // we can also end the filesystem as it will be overwritten
}
void RebootClass::endUpdateLittleFS()
{
// LittleFS has been updatet -> Now end the Webserver, Websocket and reboot
    startReboot();
}


void RebootClass::loop()
{
    if (_state == 0) {
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
            secTicker.detach();
            break;
        case 4:
            mqttTimer.detach();
            break;
        case 5:
            ModbusSmartmeterEmulation.disable();
            break;
        case 6:
            if (Config.log_sys) {
                writeEvent("INFO", "sys", "System is going to reboot", "");
            }
            DBGOUT("Rebooting...");
            break;
        case 7:
            delay(150);
            break;
        case 8:
            LittleFS.end();
            break;
        case 9:
            delay(150);
            break;
        case 10:
            //ESP.wdtDisable();           // bootet 2x ???
            ESP.restart();
            while (1) {
                delay(1);
            }
    }
}

RebootClass Reboot;

/* vim:set ts=4 et: */
