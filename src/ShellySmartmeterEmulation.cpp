#include "ShellySmartmeterEmulation.h"
#include <ArduinoJson.h>
#include "proj.h"

#define JSON_BUFFER_SIZE JSON_OBJECT_SIZE(10)

/*
  Shelly Smartmeter Emulator für B2500 Batteriespeicher. 
  Es ist nur das RPC over UDP implementiert, da dies diese Geräte nutzen. 
  Nur notwendige Werte sind gesetzt, der Rest ist Fake, reicht aber für korrekte dyn. Einspeisebegrenzung.
*/
ShellySmartmeterEmulationClass::ShellySmartmeterEmulationClass()
{
    _currentValues.dataAreValid = false;
}

void ShellySmartmeterEmulationClass::init(Device device, int offset)
{  
    //TODO check for device settings here
    _device = device;
    _offset = offset;
}

void ShellySmartmeterEmulationClass::setCurrentValues(bool dataAreValid, uint32_t v1_7_0, uint32_t v2_7_0, uint32_t v1_8_0, uint32_t v2_8_0)
{
    if (!_enabled) {
        return; // Don't waste time if there is nothing to do
    }
    _currentValues.dataAreValid = dataAreValid;
    if (!dataAreValid) {
        return; // Don't waste any more time
    }

    _currentValues.saldo = (int32_t)v1_7_0 - (int32_t)v2_7_0; //directly calculate saldo (otherwise we need some mutex around)
}

bool ShellySmartmeterEmulationClass::setEnabled(bool enabled)
{
    if (enabled) {
        enable();
    } else {
        disable();
    }
    return _enabled;
}

bool ShellySmartmeterEmulationClass::listenAndHandleUDP() {
    if(_udp.listen(_device.port)) {
        writeEvent("INFO", "emulator", F("Shelly Smartmeter Emulator Listening on Port"), String(_device.port));

        //handle request
        _udp.onPacket([&](AsyncUDPPacket packet) {
            
            if(!_currentValues.dataAreValid) {
                return;
            }

            // --- JSON parsen ---
            size_t len = packet.length();
            char tempBuffer[len+1];
            memcpy(tempBuffer, packet.data(), len);
            tempBuffer[len] = '\0';
            
            DynamicJsonBuffer jsonBuffer(JSON_BUFFER_SIZE);
            JsonObject& json = jsonBuffer.parseObject(tempBuffer);

            if (!json.success()) {
                DBGOUT("[ DEBUG ] Failed to parse json\n");
                return;
            }

            //check for objects
            if( !json.containsKey("id") || !json["id"].is<int>() || 
                !json.containsKey("method") || !json["method"].is<char*>() ||
                !json.containsKey("params")) 
            {
                DBGOUT("[ DEBUG ] Invalid json\n");
                return;
            }
            JsonObject& params = json["params"];
            if (!params.containsKey("id") || !params["id"].is<int>()) {
                DBGOUT("[ DEBUG ] Invalid json\n");
                return;
            }
            
            int id = json["id"];
            String method(json["method"]);

            //first check if deviceID is set (done here, 'cause at init Wifi may not be available already)
            if(_device.id == "") {
                _device.id = WiFi.macAddress();
                _device.id.replace(":", "");
                _device.id.toLowerCase();
                if(_device.id == 0) {
                    //fallback
                    _device.id = "aa11bb22cc33";
                }
            }
            jsonBuffer.clear();
            JsonObject& responseJson = jsonBuffer.createObject();
            responseJson["id"] = id;
            responseJson["src"] = _device.name + "-" + _device.id;
            responseJson["dst"] = "unknown";
            responseJson["result"] =  jsonBuffer.createObject();
            //the B2500 is VEEEERY picky... needs "float" formatted value with a dot
            String saldo = String(_currentValues.saldo + _offset)+".000"; 
            if(method =="EM.GetStatus") {
                responseJson["result"]["a_act_power"] = RawJson(saldo);
                responseJson["result"]["b_act_power"] = RawJson("0.000");
                responseJson["result"]["c_act_power"] = RawJson("0.000");
                responseJson["result"]["total_act_power"] = RawJson(saldo);
            } else if(method == "EM1.GetStatus") {
                responseJson["result"]["act_power"] = RawJson(saldo);
            } else {
                DBGOUT("[ DEBUG ] unknown method\n");
                return;
            }

            AsyncUDPMessage message;
            responseJson.printTo(message);
            _udp.sendTo(message, packet.remoteIP(), packet.remotePort()); //respond directly via "packet" doesn't work

        });
    
        return true;
    }

    return false;
}

bool ShellySmartmeterEmulationClass::enable(void)
{
    if(!_enabled) {
       writeEvent("INFO", "emulator", F("Shelly Smartmeter Emulation enabled"), _device.prettyName);
       if(_offset != 0) writeEvent("INFO", "emulator", F("Shelly Smartmeter Emulation using offset"), String(_offset));
       _enabled = listenAndHandleUDP();
    }
    
    return _enabled;
}

void ShellySmartmeterEmulationClass::disable(void)
{
    if(_enabled) {
        writeEvent("INFO", "emulator", F("Shelly Smartmeter Emulator disabled"), "");
        _udp.close();
    }
    _enabled = false;
}


ShellySmartmeterEmulationClass ShellySmartmeterEmulation;

/* vim:set ts=4 et: */
