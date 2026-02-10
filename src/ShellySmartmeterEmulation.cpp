#include "ShellySmartmeterEmulation.h"
#include <ArduinoJson.h>
#include "proj.h"
#include "Log.h"
#include "Network.h"
#define LOGMODULE   LOGMODULE_SHELLY
#include "unused.h"
#include <functional>
/*
  Shelly Smartmeter Emulator für B2500 Batteriespeicher.
  Es ist nur das RPC over UDP implementiert, da dies diese Geräte nutzen.
  Nur notwendige Werte sind gesetzt, der Rest ist Fake, reicht aber für korrekte dyn. Einspeisebegrenzung.
*/
ShellySmartmeterEmulationClass::ShellySmartmeterEmulationClass()
{
    _currentValues.dataAreValid = false;
}

bool ShellySmartmeterEmulationClass::init(unsigned selectedDeviceIndex, String customDeviceIDAppendix, int offset)
{
    if (selectedDeviceIndex >= std::size(DEVICES)) {
        LOGF_EP("selectedDeviceIndex out of range (%u)", selectedDeviceIndex);
        return false;
    }

    _device = DEVICES[selectedDeviceIndex];
    _offset = offset;

    //device ID not set yet
    if (customDeviceIDAppendix.isEmpty()) {
        _device.id += "-" + String(ESP.getChipId(), HEX);
    } else {
        _device.id += "-" + customDeviceIDAppendix;
    }

    _device.id.remove(26);  //known as safe lenght for the B2500
    _device.id.toLowerCase();

    return true;
}

void ShellySmartmeterEmulationClass::setCurrentValues(bool dataAreValid, uint32_t v1_7_0, uint32_t v2_7_0, uint32_t v1_8_0, uint32_t v2_8_0)
{
    UNUSED_ARG(v1_8_0);
    UNUSED_ARG(v2_8_0);

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

/*
    request looks like:
        {"id":1,"method":"EM1.GetStatus","params":{"id":0}}
    response looks like:
        {"id":1,"src":"shellyproem50-someid","dst":"unknown","result":{"act_power":100.0}}
        {"id":1,"src":"shellypro3em-someid","dst":"unknown","result":{"a_act_power":100.0, "b_act_power":100.0,"c_act_power":100.0,"total_act_power":300.0}}
*/
void ShellySmartmeterEmulationClass::handleRequest(AsyncUDPPacket udpPacket) {
    if (!_currentValues.dataAreValid) {
        return;
    }

    // --- JSON parsen ---
    size_t len = udpPacket.length();
    const size_t MAX_PACKET_SIZE = 128;  // limit valid packet size, prevents stack overflow receiving malformed packages
    if (len == 0 || len > MAX_PACKET_SIZE) {
        LOG_DP("Invalid packet size");
        return;
    }
    char tempBuffer[MAX_PACKET_SIZE + 1];
    memcpy(tempBuffer, udpPacket.data(), len);
    tempBuffer[len] = '\0';
    StaticJsonBuffer<MAX_PACKET_SIZE*2> jsonBufferRequest; //need more space for parsing, 2x surely enough for the expected small packet
    JsonObject& requestJson = jsonBufferRequest.parseObject(tempBuffer);
    if (!requestJson.success()) {
        LOG_DP("Failed to parse json");
        return;
    }

    //check for objects
    if ( !requestJson.containsKey("id") || !requestJson["id"].is<int>() ||
         !requestJson.containsKey("method") || !requestJson["method"].is<char*>() ||
         !requestJson.containsKey("params")) {
        LOG_DP("Invalid json");
        return;
    }
    JsonObject& params = requestJson["params"];
    if (!params.containsKey("id") || !params["id"].is<int>()) {
        LOG_DP("Invalid json");
        return;
    }

    int id = requestJson["id"];
    String method(requestJson["method"]);

    StaticJsonBuffer<256> jsonBufferResponse;
    JsonObject& responseJson = jsonBufferResponse.createObject();
    responseJson["id"] = id;
    responseJson["src"] = _device.id;
    responseJson["dst"] = "unknown";
    responseJson["result"] =  jsonBufferResponse.createObject();
    //the B2500 is VEEEERY picky... needs "float" formatted value with a dot
    String saldo = String(_currentValues.saldo + _offset)+".0";
    if (method =="EM.GetStatus") {
        responseJson["result"]["a_act_power"] = RawJson(saldo);
        responseJson["result"]["b_act_power"] = RawJson("0.0");
        responseJson["result"]["c_act_power"] = RawJson("0.0");
        responseJson["result"]["total_act_power"] = RawJson(saldo);
    } else if (method == "EM1.GetStatus") {
        responseJson["result"]["act_power"] = RawJson(saldo);
    } else {
        LOGF_WP("Unknown method: %s", method.c_str());
        return;
    }

    AsyncUDPMessage message;
    responseJson.printTo(message);
    _udp.sendTo(message, udpPacket.remoteIP(), udpPacket.remotePort()); //respond directly via "udpPacket" doesn't work
}

bool ShellySmartmeterEmulationClass::listen() {
    if (_udp.listen(_device.port)) {
        LOGF_IP("Shelly Smartmeter Emulator listening on port %d", _device.port);

        _udp.onPacket(std::bind(&ShellySmartmeterEmulationClass::handleRequest, this, std::placeholders::_1));

        return true;
    }

    return false;
}

bool ShellySmartmeterEmulationClass::enable(void)
{
    if (_enabled) {
        return true;
    }
    if (Network.inAPMode()) {
        return false;
    }

    LOGF_IP("Shelly Smartmeter Emulation enabled with id %s", _device.id.c_str());
    if (_offset != 0) {
        LOGF_IP("Shelly Smartmeter Emulation using offset %d W", _offset);
    }
    _enabled = listen();

    return _enabled;
}

void ShellySmartmeterEmulationClass::disable(void)
{
    if (_enabled) {
        LOG_IP("Shelly Smartmeter Emulator disabled");
        _udp.close();
    }
    _enabled = false;
}


ShellySmartmeterEmulationClass ShellySmartmeterEmulation;

/* vim:set ts=4 et: */
