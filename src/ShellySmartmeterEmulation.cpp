#include "ShellySmartmeterEmulation.h"
#include "Json.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_SHELLY
#include "Network.h"
#include "unused.h"

#include "proj.h"

#include <functional>

/*
  Shelly Smartmeter Emulator für B2500 Batteriespeicher.
  Es ist nur das RPC over UDP implementiert, da dies diese Geräte nutzen.
  Nur notwendige Werte sind gesetzt, der Rest ist Fake, reicht aber für korrekte dyn. Einspeisebegrenzung.
*/
ShellySmartmeterEmulationClass::ShellySmartmeterEmulationClass()
{
    using std::placeholders::_1;
    _udp.onPacket(std::bind(&ShellySmartmeterEmulationClass::handleRequest, this, _1));

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
        {"id":1,"method":"EM.GetStatus","params":{"id":0}}
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
    constexpr size_t MIN_PACKET_SIZE = 48;   // limit valid packet size, prevents stack overflow receiving malformed packages
    constexpr size_t MAX_PACKET_SIZE = 128;
    if (len < MIN_PACKET_SIZE || len > MAX_PACKET_SIZE) {
        LOGF_DP("Invalid packet size %u", len);
        return;
    }

    constexpr size_t ADDITIONAL_JSON_OBJECTS = 4; // as we don't have full control over json request: allow 4 objects more
    StaticJsonDocument<JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(ADDITIONAL_JSON_OBJECTS)> requestJson;
    DeserializationError error = deserializeJson(requestJson, (char*)udpPacket.data(), len);
    if (error) {
        LOGF_EP("Failed to parse json. Error '%s'", error.c_str());
        return;
    }

    //check for objects
    if ( !requestJson.containsKey("id") || !requestJson["id"].is<JsonInteger>() ||
         !requestJson.containsKey("method") || !requestJson["method"].is<const char*>() ||
         !requestJson.containsKey("params" )) {
        LOG_EP("Invalid json #1.");
        return;
    }
    JsonObject params = requestJson["params"];
    if (!params.containsKey("id") || !params["id"].is<JsonInteger>()) {
        LOG_EP("Invalid json #2.");
        return;
    }

    const char *method = requestJson["method"];
    int id = requestJson["id"];

    StaticJsonDocument<260> responseJson;
    responseJson["id"] = id;
    responseJson["src"] = _device.id;
    responseJson["dst"] = "unknown";
    JsonObject result = responseJson.createNestedObject("result");

    //the B2500 is VEEEERY picky... needs "float" formatted value with a dot
    String saldo = String(_currentValues.saldo + _offset) + ".0";
    if (!strcmp(method, "EM.GetStatus")) {
        result["a_act_power"] = serialized(saldo);
        result["b_act_power"] = serialized("0.0");
        result["c_act_power"] = serialized("0.0");
        result["total_act_power"] = serialized(saldo);
    } else if (!strcmp(method, "EM1.GetStatus")) {
        result["act_power"] = serialized(saldo);
    } else {
        LOGF_WP("Unknown method: %s", method);
        return;
    }

    AsyncUDPMessage message;
    SERIALIZE_JSON_LOG(responseJson, message);
    _udp.sendTo(message, udpPacket.remoteIP(), udpPacket.remotePort()); //respond directly via "udpPacket" doesn't work
}

bool ShellySmartmeterEmulationClass::listen() {
    if (_udp.listen(_device.port)) {
        return true;
    }
    LOGF_EP("Starting listener on port %d failed", _device.port);
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

    LOGF_IP("Starting listening on port %d, id '%s' offset %d W", _device.port, _device.id.c_str(), _offset);
    _enabled = listen();

    return _enabled;
}

void ShellySmartmeterEmulationClass::disable(void)
{
    if (_enabled) {
        _udp.close();
        LOG_IP("disabled");
    }
    _enabled = false;
}


ShellySmartmeterEmulationClass ShellySmartmeterEmulation;

/* vim:set ts=4 et: */
