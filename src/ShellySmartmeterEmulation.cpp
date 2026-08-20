#include "ShellySmartmeterEmulation.h"
#include "config.h"
#include "Json.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_SHELLY
#include "Network.h"

#include "proj.h"

#include <ESP8266WiFi.h>
#include <functional>

typedef struct {
    uint16_t port;              // Port of the "RPC over UDP" API
    const char *pstr_id;        // Prefix of the device id, also announced as mDNS TXT record "app"
    const char *pstr_name;      // Prefix of the device name (mDNS instance name), mixed case
    const char *pstr_app;       // Application name as reported by "Shelly.GetDeviceInfo"
    const char *pstr_model;     // Model name as reported by "Shelly.GetDeviceInfo" and via mDNS
    uint8_t generation;         // Shelly device generation as reported by "Shelly.GetDeviceInfo"
    uint8_t mdnsGeneration;     // Shelly device generation as announced via mDNS (see note below)
    bool triphase;              // true: 3 phase device (EM.*), false: single phase device (EM1.*)
} _devicesAvailable_t;

//NOTE: be careful, index of array must match the selected dropbox value of index.html
const char ID_EM50[] PROGMEM = "shellyproem50";
const char ID_EMG3[] PROGMEM = "shellyemg3";
const char ID_P3EM[] PROGMEM = "shellypro3em";

// The device name uses the "official" spelling, the device id is its lowercase variant
const char NAME_EM50[] PROGMEM = "ShellyProEM50";
const char NAME_EMG3[] PROGMEM = "ShellyEMG3";
const char NAME_P3EM[] PROGMEM = "ShellyPro3EM";

const char APP_EM50[] PROGMEM = "PROEM50";
const char APP_EMG3[] PROGMEM = "S3EM";
const char APP_P3EM[] PROGMEM = "Pro3EM";

const char MODEL_EM50[] PROGMEM = "SPEM-002CEBEM50";
const char MODEL_EMG3[] PROGMEM = "S3EM-002CXCEU";
const char MODEL_P3EM[] PROGMEM = "SPEM-003CEBEU";

/*
  Firmware version we pretend to run. Some consumers check these values, so we report
  what a real device would report (the values are the ones the "uni-meter" project uses,
  they are known to work with a Solakon One).
*/
const char SHELLY_FW_ID[] PROGMEM = "20250924-062729/1.7.1-gd336f31";
const char SHELLY_FW_VERSION[] PROGMEM = "1.7.1";

/*
  NOTE on the Pro 3EM announcing "gen=3" via mDNS although it is a gen 2 device:
  That's what makes a Solakon One accept the emulation and it is the value the
  "uni-meter" project ships as default too.
  See https://github.com/sdeigm/uni-meter/issues/267
*/
static const _devicesAvailable_t _DEVICES[4] = {
    { 2223, ID_EM50, NAME_EM50, APP_EM50, MODEL_EM50, 2, 2, false },  //Shelly Pro EM-50
    { 2222, ID_EMG3, NAME_EMG3, APP_EMG3, MODEL_EMG3, 3, 3, false },  //Shelly EM Gen3
    { 2220, ID_P3EM, NAME_P3EM, APP_P3EM, MODEL_P3EM, 2, 3, true  },  //Shelly Pro 3EM (Firmware >=224)
    { 1010, ID_P3EM, NAME_P3EM, APP_P3EM, MODEL_P3EM, 2, 3, true  }   //Shelly Pro 3EM (Firmware <224)
};

// Nominal grid values. The AMIS meter does not report them, but a lot of consumers
// refuse to work with a voltage or frequency of 0.
static constexpr float NOMINAL_VOLTAGE = 230.0f;
static constexpr float NOMINAL_FREQUENCY = 50.0f;


/*
  Shelly Smartmeter Emulator.

  Two APIs are provided:
    - "RPC over UDP" (this file, see handleRequest()) for B2500 batteries (Marstek Saturn, ...)
    - "RPC over HTTP" (see Webserver_Shelly.cpp) plus the mDNS announcement (see Network.cpp)
      for consumers which discover and query the Shelly the way the real device does
      (Solakon One, Hoymiles, Anker, ...)

  Both use the same response data - only the transport and the amount of returned values differ.
  Only necessary values are set, the rest is fake, but that's enough for a correct
  dynamic feed-in limitation.
*/
ShellySmartmeterEmulationClass::ShellySmartmeterEmulationClass()
{
    using std::placeholders::_1;
    _udp.onPacket(std::bind(&ShellySmartmeterEmulationClass::handleRequest, this, _1));

    _device.name[0] = '\0';
    _device.id[0] = '\0';
    _currentValues.dataAreValid = false;
}

bool ShellySmartmeterEmulationClass::init(unsigned selectedDeviceIndex, const char *customDeviceIDAppendix, int offset)
{
    if (selectedDeviceIndex >= std::size(_DEVICES)) {
        LOGF_EP("selectedDeviceIndex out of range (%u)", selectedDeviceIndex);
        return false;
    }

    if (customDeviceIDAppendix[0]) {
        snprintf_P(_device.name, sizeof(_device.name), PSTR("%S-%s"), _DEVICES[selectedDeviceIndex].pstr_name, customDeviceIDAppendix);
    } else {
        // A real Shelly uses its mac address - do the same, some consumers are picky
        snprintf_P(_device.name, sizeof(_device.name), PSTR("%S-%s"), _DEVICES[selectedDeviceIndex].pstr_name, getMacAddress().c_str());
    }
    // The id is the lowercase variant of the name (that's how a real Shelly does it too)
    strlcpy(_device.id, _device.name, sizeof(_device.id));
    for (char *c = _device.id; *c; c++) {
        *c = tolower(*c);
    }
    _device.port = _DEVICES[selectedDeviceIndex].port;
    _deviceIndex = selectedDeviceIndex;
    _offset = offset;

    return true;
}

const __FlashStringHelper *ShellySmartmeterEmulationClass::getApp() const
{
    return FPSTR(_DEVICES[_deviceIndex].pstr_app);
}

const __FlashStringHelper *ShellySmartmeterEmulationClass::getMdnsApp() const
{
    return FPSTR(_DEVICES[_deviceIndex].pstr_id);
}

const __FlashStringHelper *ShellySmartmeterEmulationClass::getModel() const
{
    return FPSTR(_DEVICES[_deviceIndex].pstr_model);
}

unsigned ShellySmartmeterEmulationClass::getGeneration() const
{
    return _DEVICES[_deviceIndex].generation;
}

unsigned ShellySmartmeterEmulationClass::getMdnsGeneration() const
{
    return _DEVICES[_deviceIndex].mdnsGeneration;
}

bool ShellySmartmeterEmulationClass::isTriphase() const
{
    return _DEVICES[_deviceIndex].triphase;
}

const __FlashStringHelper *ShellySmartmeterEmulationClass::getFirmwareId()
{
    return FPSTR(SHELLY_FW_ID);
}

const __FlashStringHelper *ShellySmartmeterEmulationClass::getFirmwareVersion()
{
    return FPSTR(SHELLY_FW_VERSION);
}

String ShellySmartmeterEmulationClass::getMacAddress()
{
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toUpperCase();
    return mac;
}

void ShellySmartmeterEmulationClass::setCurrentValues(bool dataAreValid, uint32_t v1_7_0, uint32_t v2_7_0, uint32_t v1_8_0, uint32_t v2_8_0)
{
    if (!_enabled && !_httpApiEnabled) {
        return; // Don't waste time if there is nothing to do
    }
    _currentValues.dataAreValid = dataAreValid;
    if (!dataAreValid) {
        return; // Don't waste any more time
    }

    _currentValues.saldo = (int32_t)v1_7_0 - (int32_t)v2_7_0; //directly calculate saldo (otherwise we need some mutex around)
    _currentValues.v1_8_0 = v1_8_0;
    _currentValues.v2_8_0 = v2_8_0;
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
    Builds the "result" object of a RPC response.

    'minimalResponse' is used by the UDP API: the B2500 batteries are picky and the
    minimal response is a proven one - so don't extend it there.
*/
bool ShellySmartmeterEmulationClass::buildRpcResult(const char *method, JsonObject result, bool minimalResponse)
{
    if (!strcmp_P(method, PSTR("EM.GetStatus"))) {
        if (!_currentValues.dataAreValid) {
            return false;
        }
        buildEmGetStatus(result, minimalResponse);
        return true;
    }
    if (!strcmp_P(method, PSTR("EM1.GetStatus"))) {
        if (!_currentValues.dataAreValid) {
            return false;
        }
        buildEm1GetStatus(result, minimalResponse);
        return true;
    }
    if (minimalResponse) {
        // Everything below is only answered on the HTTP API
        return false;
    }
    if (!strcmp_P(method, PSTR("EMData.GetStatus"))) {
        if (!_currentValues.dataAreValid) {
            return false;
        }
        buildEmDataGetStatus(result);
        return true;
    }
    if (!strcmp_P(method, PSTR("EM1Data.GetStatus"))) {
        if (!_currentValues.dataAreValid) {
            return false;
        }
        buildEm1DataGetStatus(result);
        return true;
    }
    if (!strcmp_P(method, PSTR("EM.GetConfig")) || !strcmp_P(method, PSTR("EM1.GetConfig"))) {
        buildEmGetConfig(result);
        return true;
    }
    if (!strcmp_P(method, PSTR("Shelly.GetDeviceInfo"))) {
        buildDeviceInfo(result);
        return true;
    }
    if (!strcmp_P(method, PSTR("Shelly.ListMethods"))) {
        buildListMethods(result);
        return true;
    }

    return false;
}

/*
    https://shelly-api-docs.shelly.cloud/gen2/ComponentsAndServices/EM#emgetstatus-example

    The saldo is reported on phase A only - splitting it up over all three phases would
    be a fake which nobody asked for (see discussion in issue #36).
*/
void ShellySmartmeterEmulationClass::buildEmGetStatus(JsonObject result, bool minimalResponse)
{
    int32_t saldoW = saldo();

    if (minimalResponse) {
        //the B2500 is VEEEERY picky... needs "float" formatted value with a dot
        String saldoMinimal = String(saldoW) + ".0";
        result["a_act_power"] = serialized(saldoMinimal);
        result["b_act_power"] = serialized("0.0");
        result["c_act_power"] = serialized("0.0");
        result["total_act_power"] = serialized(saldoMinimal);
        return;
    }

    String saldoStr = String(saldoW) + ".00";

    String current = String(saldoW / NOMINAL_VOLTAGE, 2);
    String apparent = String(saldoW < 0 ? -(float)saldoW : (float)saldoW, 2);
    String voltage = String(NOMINAL_VOLTAGE, 2);
    String frequency = String(NOMINAL_FREQUENCY, 2);

    result["id"] = 0;

    result["a_current"] = serialized(current);
    result["a_voltage"] = serialized(voltage);
    result["a_act_power"] = serialized(saldoStr);
    result["a_aprt_power"] = serialized(apparent);
    result["a_pf"] = serialized("1.00");
    result["a_freq"] = serialized(frequency);

    result["b_current"] = serialized("0.00");
    result["b_voltage"] = serialized(voltage);
    result["b_act_power"] = serialized("0.00");
    result["b_aprt_power"] = serialized("0.00");
    result["b_pf"] = serialized("1.00");
    result["b_freq"] = serialized(frequency);

    result["c_current"] = serialized("0.00");
    result["c_voltage"] = serialized(voltage);
    result["c_act_power"] = serialized("0.00");
    result["c_aprt_power"] = serialized("0.00");
    result["c_pf"] = serialized("1.00");
    result["c_freq"] = serialized(frequency);

    result["n_current"] = nullptr;
    result["total_current"] = serialized(current);
    result["total_act_power"] = serialized(saldoStr);
    result["total_aprt_power"] = serialized(apparent);
    result.createNestedArray("user_calibrated_phase");
}

/*
    https://shelly-api-docs.shelly.cloud/gen2/ComponentsAndServices/EM1#em1getstatus-example
*/
void ShellySmartmeterEmulationClass::buildEm1GetStatus(JsonObject result, bool minimalResponse)
{
    int32_t saldoW = saldo();

    if (minimalResponse) {
        //the B2500 is VEEEERY picky... needs "float" formatted value with a dot
        result["act_power"] = serialized(String(saldoW) + ".0");
        return;
    }

    String saldoStr = String(saldoW) + ".00";
    String current = String(saldoW / NOMINAL_VOLTAGE, 2);

    result["id"] = 0;
    result["current"] = serialized(current);
    result["voltage"] = serialized(String(NOMINAL_VOLTAGE, 2));
    result["act_power"] = serialized(saldoStr);
    result["aprt_power"] = serialized(String(saldoW < 0 ? -(float)saldoW : (float)saldoW, 2));
    result["pf"] = serialized("1.00");
    result["freq"] = serialized(String(NOMINAL_FREQUENCY, 2));
    result["calibration"] = F("factory");
}

/*
    https://shelly-api-docs.shelly.cloud/gen2/ComponentsAndServices/EMData#emdatagetstatus-example

    Energy counters are reported in Wh - that's exactly what the meter delivers in 1.8.0/2.8.0.
*/
void ShellySmartmeterEmulationClass::buildEmDataGetStatus(JsonObject result)
{
    String consumed = String(_currentValues.v1_8_0) + ".00";
    String returned = String(_currentValues.v2_8_0) + ".00";

    result["id"] = 0;
    result["a_total_act_energy"] = serialized(consumed);
    result["a_total_act_ret_energy"] = serialized(returned);
    result["b_total_act_energy"] = serialized("0.00");
    result["b_total_act_ret_energy"] = serialized("0.00");
    result["c_total_act_energy"] = serialized("0.00");
    result["c_total_act_ret_energy"] = serialized("0.00");
    result["total_act"] = serialized(consumed);
    result["total_act_ret"] = serialized(returned);
}

/*
    https://shelly-api-docs.shelly.cloud/gen2/ComponentsAndServices/EM1Data#em1datagetstatus-example
*/
void ShellySmartmeterEmulationClass::buildEm1DataGetStatus(JsonObject result)
{
    result["id"] = 0;
    result["total_act_energy"] = serialized(String(_currentValues.v1_8_0) + ".00");
    result["total_act_ret_energy"] = serialized(String(_currentValues.v2_8_0) + ".00");
}

/*
    https://shelly-api-docs.shelly.cloud/gen2/ComponentsAndServices/EM#emgetconfig-example
*/
void ShellySmartmeterEmulationClass::buildEmGetConfig(JsonObject result)
{
    result["id"] = 0;
    result["name"] = nullptr;
    result["blink_mode_selector"] = F("active_energy");
    result["phase_selector"] = F("a");
    result["monitor_phase_sequence"] = false;
    result["ct_type"] = F("120A");
}

/*
    https://shelly-api-docs.shelly.cloud/gen2/ComponentsAndServices/Shelly#shellygetdeviceinfo-example
*/
void ShellySmartmeterEmulationClass::buildDeviceInfo(JsonObject result)
{
    result["name"] = Config.DeviceName;
    result["id"] = _device.name;
    result["mac"] = getMacAddress();
    result["slot"] = 1;
    result["model"] = getModel();
    result["gen"] = getGeneration();
    result["fw_id"] = getFirmwareId();
    result["ver"] = getFirmwareVersion();
    result["app"] = getApp();
    result["auth_en"] = false;
    result["auth_domain"] = nullptr;
    result["profile"] = isTriphase() ? F("triphase") : F("monophase");
}

/*
    https://shelly-api-docs.shelly.cloud/gen2/ComponentsAndServices/Shelly#shellylistmethods-example
*/
void ShellySmartmeterEmulationClass::buildListMethods(JsonObject result)
{
    JsonArray methods = result.createNestedArray("methods");
    if (isTriphase()) {
        methods.add(F("EM.GetConfig"));
        methods.add(F("EM.GetStatus"));
        methods.add(F("EMData.GetStatus"));
    } else {
        methods.add(F("EM1.GetConfig"));
        methods.add(F("EM1.GetStatus"));
        methods.add(F("EM1Data.GetStatus"));
    }
    methods.add(F("Shelly.GetDeviceInfo"));
    methods.add(F("Shelly.ListMethods"));
}


/*
    "RPC over UDP"

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

    // Keep the UDP responses minimal - that's what the B2500 batteries expect
    if (!buildRpcResult(method, result, true)) {
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

    LOGF_IP("Starting listening on port %d, id '%s' offset %d W", _device.port, _device.id, _offset);
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
    disableHttpApi();
}

bool ShellySmartmeterEmulationClass::enableHttpApi(void)
{
    if (_httpApiEnabled) {
        return true;
    }
    if (Network.inAPMode() || !isInitialized()) {
        return false;
    }

    LOGF_IP("Starting HTTP API, id '%s' offset %d W", _device.id, _offset);
    _httpApiEnabled = true;

    return _httpApiEnabled;
}

void ShellySmartmeterEmulationClass::disableHttpApi(void)
{
    if (_httpApiEnabled) {
        LOG_IP("HTTP API disabled");
    }
    _httpApiEnabled = false;
}


ShellySmartmeterEmulationClass ShellySmartmeterEmulation;

/* vim:set ts=4 et: */
