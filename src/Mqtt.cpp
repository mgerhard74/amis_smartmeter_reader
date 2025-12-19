#include "Mqtt.h"

#include "AmisReader.h"
#include "config.h"
#include "DefaultConfigurations.h"
#include "Network.h"
#include "unused.h"

#include <AsyncJson.h>
#include <LittleFS.h>

//#define DEBUG
#include "debug.h"

#ifdef STROMPREIS
extern String strompreis;
#endif


#include "proj.h"
extern unsigned int first_frame;
extern void writeEvent(String, String, String, String);


// Helper: sanitize a device name for topic/object id
static String sanitizeTopic(const String &s)
{
    String t = s;
    for (unsigned int i = 0; i < t.length(); i++) {
        char c = t[i];
        if (!isalnum(c)) {
            t[i] = '_';
        } else {
            t[i] = tolower(c);
        }
    }
    return t;
}


static String get_ha_availability_topic(MqttConfig_t &configMqtt)
{
    String avail_topic;
    if (!configMqtt.mqtt_pub.isEmpty()) {
        avail_topic = configMqtt.mqtt_pub;
        if (!avail_topic.endsWith("/")) {
            avail_topic += "/";
        }
        avail_topic += "status";
    } else {
        avail_topic = sanitizeTopic(Config.DeviceName) + String("/status");
    }
    return avail_topic;
}


bool MqttClass::loadConfigMqtt(MqttConfig_t &config)
{
    File configFile = LittleFS.open("/config_mqtt", "r");
    if (!configFile) {
        DBGOUT(F("[ WARN ] Failed to open config_mqtt\n"));
#ifndef DEFAULT_CONFIG_MQTT_JSON
        return false;
#endif
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject *json = nullptr;
    if (configFile) {
        json = &jsonBuffer.parseObject(configFile);
        configFile.close();
    } else {
#ifdef DEFAULT_CONFIG_MQTT_JSON
        json = &jsonBuffer.parseObject(DEFAULT_CONFIG_MQTT_JSON);
#endif
    }
    if (json == nullptr || !json->success()) {
        DBGOUT(F("[ WARN ] Failed to parse config_mqtt\n"));
        return false;
    }
    ///json.prettyPrintTo(Serial);
    config.mqtt_qos = (*json)[F("mqtt_qos")].as<uint8_t>();
    config.mqtt_retain = (*json)[F("mqtt_retain")].as<bool>();
    config.mqtt_sub = (*json)[F("mqtt_sub")].as<String>();
    config.mqtt_pub = (*json)[F("mqtt_pub")].as<String>();
    config.mqtt_keep = (*json)[F("mqtt_keep")].as<unsigned int>();
    config.mqtt_ha_discovery = (*json)[F("mqtt_ha_discovery")].as<bool>();
    config.mqtt_will = (*json)[F("mqtt_will")].as<String>();
    config.mqtt_user = (*json)[F("mqtt_user")].as<String>();
    config.mqtt_password = (*json)[F("mqtt_password")].as<String>();
    config.mqtt_client_id = (*json)[F("mqtt_clientid")].as<String>();
    config.mqtt_enabled = (*json)[F("mqtt_enabled")].as<bool>();
    config.mqtt_broker = (*json)[F("mqtt_broker")].as<String>();
    config.mqtt_port = (*json)[F("mqtt_port")].as<uint16_t>();

    // Some value checks
    if (config.mqtt_keep == 0) {
        // im Webinterface: 1...x / 30 = default
        config.mqtt_keep = 30;
    }
    if (config.mqtt_port == 0) {
        // im Webinterface: 1...65535 / 1883 = default
        config.mqtt_port = 1883;
    }

    return true;
}


MqttClass::MqttClass()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    _client.onMessage(std::bind(&MqttClass::onMessage, this, _1, _2, _3, _4, _5, _6));
    _client.onConnect(std::bind(&MqttClass::onConnect, this, _1));
    _client.onDisconnect(std::bind(&MqttClass::onDisconnect, this, _1));
    _client.onPublish(std::bind(&MqttClass::onPublish, this, _1));
}


void MqttClass::init()
{
    loadConfigMqtt(_config);
    _reloadConfigState = 0;
}


void MqttClass::onConnect(bool sessionPresent)
{
    UNUSED_ARG(sessionPresent);

    _ticker.detach();
    if (_config.mqtt_keep) {
        _ticker.attach_scheduled(_config.mqtt_keep, std::bind(&MqttClass::aliveTicker, this));
    }
    eprintf("MQTT onConnect %u %s\n", sessionPresent, Config.mqtt_sub.c_str());
    if (Config.log_sys) {
        writeEvent("INFO", "mqtt", "Connected to MQTT Server", "sessionPresent=" + String(sessionPresent));
    }
    if (!_config.mqtt_sub.isEmpty()) {
        _client.subscribe(_config.mqtt_sub.c_str(), _config.mqtt_qos);
        eprintf("MQTT subscr %s\n", _config.mqtt_sub.c_str());
    }
    if (_config.mqtt_ha_discovery) {
        // Publish 'online' to availability topic (birth) so Home Assistant / other clients see device is online
        publishHaAvailability(true);
        // Publish Home Assistant discovery info so HA can auto-detect this device (if enabled)
        publishHaDiscovery();
    }
#ifdef STROMPREIS
    _client.subscribe("strompreis",0);
#endif // STROMPREIS
}


void MqttClass::publishState()
{
    if (!_client.connected()) {
        DBGOUT("MQTT publish: not connected\n");
        return;
    }

    if (first_frame !=1 ) {
        return;
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    signed saldo = a_result[4] - a_result[5] - Config.rest_ofs;
    if (Config.rest_neg) {
        saldo =-saldo;
    }
    if (Config.rest_var == 0) {
        // Variablennamen mit Punkten (".")
        root[F("1.8.0")] = a_result[0];
        root[F("2.8.0")] = a_result[1];
        root[F("3.8.1")] = a_result[2];
        root[F("4.8.1")] = a_result[3];
        root[F("1.7.0")] = a_result[4];
        root[F("2.7.0")] = a_result[5];
        root[F("3.7.0")] = a_result[6];
        root[F("4.7.0")] = a_result[7];
#ifdef STROMPREIS
        root[F("strompreis")]=strompreis;
#else
        root[F("1.128.0")] = a_result[8];
#endif
    } else {
        // Variablennamen mit Unterstrichen ("_")
        root[F("1_8_0")] = a_result[0];
        root[F("2_8_0")] = a_result[1];
        root[F("3_8_1")] = a_result[2];
        root[F("4_8_1")] = a_result[3];
        root[F("1_7_0")] = a_result[4];
        root[F("2_7_0")] = a_result[5];
        root[F("3_7_0")] = a_result[6];
        root[F("4_7_0")] = a_result[7];
        root[F("1_128_0")] = a_result[8];
    }
    root[F("saldo")] = saldo;
    root[F("time")] = a_result[9];
    root[F("serialnumber")] = AmisReader.getSerialNumber();

    String mqttBuffer;
    //root.prettyPrintTo(mqttBuffer);
    root.printTo(mqttBuffer);
    _client.publish(_config.mqtt_pub.c_str(), _config.mqtt_qos, _config.mqtt_retain, mqttBuffer.c_str());
    //DBGOUT("MQTT publish "+Config.mqtt_pub+": "+mqttBuffer+"\n");
}


void MqttClass::aliveTicker() {
    if (valid == 5) {
        publishState();
    }
}


void MqttClass::connect() {
    if (!_config.mqtt_enabled) {
        return;
    }

    IPAddress ipAddr;
    String mqttServer;
    if (ipAddr.fromString(_config.mqtt_broker) && ipAddr.isSet()) {
        eprintf("MQTT init: %s %d\n", ipAddr.toString().c_str(), Config.mqtt_port);
        _client.setServer(ipAddr, _config.mqtt_port);
        mqttServer = ipAddr.toString();
    } else {
        eprintf("MQTT init: %s %d\n", Config.mqtt_broker.c_str(), Config.mqtt_port);
        _client.setServer(_config.mqtt_broker.c_str(), _config.mqtt_port);
        mqttServer = _config.mqtt_broker;
    }

    if (!_config.mqtt_will.isEmpty()) {
        _client.setWill(_config.mqtt_will.c_str(), _config.mqtt_qos, _config.mqtt_retain, Config.DeviceName.c_str());
        eprintf("MQTT SetWill: %s %u %u %s\n", _config.mqtt_will.c_str(), _config.mqtt_qos, _config.mqtt_retain, Config.DeviceName.c_str());
    } else if (_config.mqtt_ha_discovery) {
        // Set LWT to availability topic for HA discovery in case the user did not define a custom one
        String avail_topic = get_ha_availability_topic(_config);
        // mqttClient.setWill(avail_topic.c_str(), Config.mqtt_qos, true, String("offline").c_str());
        eprintf("MQTT SetWill (HA): %s %u %u offline\n", avail_topic.c_str(), Config.mqtt_qos, true);
    }
    if (!_config.mqtt_user.isEmpty()) {
        _client.setCredentials(_config.mqtt_user.c_str(), _config.mqtt_password.c_str());
        eprintf("MQTT User: %s %s\n", _config.mqtt_user.c_str(), _config.mqtt_password.c_str());
    }
    if (!_config.mqtt_client_id.isEmpty()) {
        _client.setClientId(_config.mqtt_client_id.c_str());
        eprintf("MQTT ClientId: %s\n", _config.mqtt_client_id.c_str());
    }

    if (Config.log_sys) {
        writeEvent("INFO", "mqtt", "Connecting to MQTT server " + mqttServer, "...");
    }
    _client.connect();
}


void MqttClass::onDisconnect(AsyncMqttClientDisconnectReason reason) {
    if (_reloadConfigState == 0) {
        _ticker.detach();
    }
    String reasonstr = "";
    switch (reason) {
    case(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED):
        reasonstr = F("TCP_DISCONNECTED");
        break;
    case(AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION):
        reasonstr = F("MQTT_UNACCEPTABLE_PROTOCOL_VERSION");
        break;
    case(AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED):
        reasonstr = F("MQTT_IDENTIFIER_REJECTED");
        break;
    case(AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE):
        reasonstr = F("MQTT_SERVER_UNAVAILABLE");
        break;
    case(AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS):
        reasonstr = F("MQTT_MALFORMED_CREDENTIALS");
        break;
    case(AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED):
        reasonstr = F("MQTT_NOT_AUTHORIZED");
        break;
    case(AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE):
        reasonstr = F("ESP8266_NOT_ENOUGH_SPACE");
        break;
    default:
        reasonstr = F("Unknown");
        break;
    }
    if (Config.log_sys) {
        writeEvent("WARN", "mqtt", "Disconnected from MQTT server", reasonstr);
    }
    eprintf("Disconnected from MQTT server: %s\n", reasonstr.c_str());

    // If we have HA discovery enabled, try to publish offline status (clean disconnect)
    if (_config.mqtt_ha_discovery) {
        publishHaAvailability(false);
    }

    _ticker.attach_scheduled(15, std::bind(&MqttClass::connect, this));
}


void MqttClass::onPublish(uint16_t packetId) {
// not working!!!
    DBGOUT ("onMqttPublish\n");
    if (Config.log_sys) {
        writeEvent("INFO", "mqtt", "MQTT publish acknowledged", String(packetId));
    }
}


void MqttClass::publishHaAvailability(bool isOnline) {
    if (!_client.connected()) {
        return;
    }
    String avail_topic = get_ha_availability_topic(_config);
    String payload = isOnline ? "online" : "offline";
    _client.publish(avail_topic.c_str(), _config.mqtt_qos, true, payload.c_str());
}

// Publish discovery for all relevant measurement keys. Use bracket notation
// for JSON access to handle keys with dots or underscores.
struct HASensor {
    const char *key;
    const char *name;
    const char *unit;
    const char *device_class;
    const char *state_class;
};

// Helper that accepts the whole HASensor and builds the discovery payload,
// including rest_var handling and value_template logic.
static void publishSensor(AsyncMqttClient &client, const HASensor &e, String &dev, MqttConfig_t &configMqtt)
{
    //String dev = sanitizeTopic(Config.DeviceName + String("_") + chipId);

    // choose key variant depending on rest_var (dots vs underscores)
    String key_use = String(e.key);
    if (Config.rest_var != 0) {
        key_use.replace('.', '_');
    }

    // Build the value_template: energy entries are in Wh -> convert to kWh (/1000)
    String tpl;
    if (e.device_class && strcmp(e.device_class, "energy") == 0) {
        tpl = String("{{ (value_json[\"") + key_use + String("\"] / 1000) }}");
    } else {
        tpl = String("{{ value_json[\"") + key_use + String("\"] }}");
    }

    // Build the discovery JSON
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    String name = String(Config.DeviceName) + " " + e.name;
    root[F("name")] = name;
    root[F("state_topic")] = configMqtt.mqtt_pub;
    root[F("availability_topic")] = get_ha_availability_topic(configMqtt);
    // root[F("payload_available")] = "online";
    // root[F("payload_not_available")] = "offline";

    if (e.unit && e.unit[0]) {
        root[F("unit_of_measurement")] = e.unit;
    }
    if (!tpl.isEmpty()) {
        root[F("value_template")] = tpl;
    }
    String obj = sanitizeTopic(key_use);
    String uid = dev + String("_") + obj;
    root[F("unique_id")] = uid;
    // attach device object
    JsonObject &devn = root.createNestedObject("device");
    JsonArray &ids = devn.createNestedArray("identifiers");
    ids.add(Config.DeviceName);
    devn[F("name")] = Config.DeviceName;
    devn[F("model")] = APP_NAME;
    devn[F("sw_version")] = VERSION;
    if (e.device_class && e.device_class[0]) {
        root[F("device_class")] = e.device_class;
    }
    if (e.state_class && e.state_class[0]) {
        root[F("state_class")] = e.state_class;
    }

    String out;
    root.printTo(out);
    String topic = String("homeassistant/sensor/") + dev + String("/") + obj + String("/config");
    client.publish(topic.c_str(), configMqtt.mqtt_qos, true, out.c_str());
};


// Publish Home Assistant MQTT discovery messages for all sensors
void MqttClass::publishHaDiscovery() {
    if (!_client.connected()) {
        return;
    }

    // Use the configured mqtt_pub as the main state_topic for sensors
    if (_config.mqtt_pub.isEmpty()) {
        return;
    }

    HASensor entries[] = {
        {"1.8.0","Bezug","kWh","energy","total_increasing"},
        {"2.8.0","Lieferung","kWh","energy","total_increasing"},
        {"1.7.0","Leistung Bezug","W","power","measurement"},
        {"2.7.0","Leistung Lieferung","W","power","measurement"},
        {"saldo","Saldo","W", "power","measurement"},
    };

    String dev = sanitizeTopic(Config.DeviceName + String("_") + String(ESP.getChipId(), HEX));

    for (size_t i=0; i < sizeof(entries)/sizeof(entries[0]); i++) {
        publishSensor(_client, entries[i], dev, _config);
    }
}


void MqttClass::onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    UNUSED_ARG(topic);
    UNUSED_ARG(payload);
    UNUSED_ARG(properties);
    UNUSED_ARG(len);
    UNUSED_ARG(index);
    UNUSED_ARG(total);
#ifdef STROMPREIS
    char p[20];
    memcpy(p,payload,len);
    p[len]=0;
    strompreis = String(p);
    //DBGOUT(strompreis+"\n");
#endif // STROMPREIS
}


void MqttClass::networkOnStationModeGotIP(const WiFiEventStationModeGotIP& event)
{
    UNUSED_ARG(event);

    start();
}


void MqttClass::networkOnStationModeDisconnected(const WiFiEventStationModeDisconnected& event)
{
    UNUSED_ARG(event);

    stop(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
}

void MqttClass::stop()
{
    _ticker.detach();
    if (_client.connected()) {
        _client.disconnect();
    }
}

void MqttClass::start()
{
    _ticker.detach();

    if (!_config.mqtt_enabled) {
        return;
    }

    if (_config.mqtt_broker.isEmpty()) {
        return;
    }

    _ticker.once_scheduled(15, std::bind(&MqttClass::connect, this));
}


bool MqttClass::isConnected()
{
    return _client.connected();
}


void MqttClass::reloadConfig() {
    if (_reloadConfigState == 0) {
        stop();
        _reloadConfigState = 1;
        _ticker.attach_ms(500, std::bind(&MqttClass::reloadConfig, this));
    } else if (_reloadConfigState == 1) {
        if (_client.connected()) {
            return;
        }
        loadConfigMqtt(_config);
        _reloadConfigState = 2;
        if (Config.log_sys) {
            writeEvent("INFO", "mqtt", "Config reloaded", "");
        }
    }  else if (_reloadConfigState == 2) {
        _reloadConfigState = 0;
        if (Network.isConnected()) {
            start();
        } else {
            _ticker.detach();
        }
    }
}

/*
const MqttConfig_t &MqttClass::getConfigMqtt(void)
{
    return _config;
}
*/

MqttClass Mqtt;

/* vim:set ts=4 et: */
