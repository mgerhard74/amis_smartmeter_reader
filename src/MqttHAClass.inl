// Publiziert für HomeAssistant die verfügbaren Sensor Endpunkte
// sowie den Status und ein paar Eckdaten (APP_VERSION, ...)
//
// Liefert aktuell keine Zählerdaten oder Echtzeitwerte aus

#include "ProjectConfiguration.h"

#include "Mqtt.h"

#include "config.h"


struct HASensor {
    const char *key;
    const char *name;
    const char *unit;
    const char *device_class;
    const char *state_class;
};

// Publish discovery for all relevant measurement keys. Use bracket notation
// for JSON access to handle keys with dots or underscores.
static const HASensor HASensors[] = {
    {"1.8.0",   "Bezug",                "kWh",  "energy",   "total_increasing"},
    {"2.8.0",   "Lieferung",            "kWh",  "energy",   "total_increasing"},
    {"1.7.0",   "Leistung Bezug",       "W",    "power",    "measurement"},
    {"2.7.0",   "Leistung Lieferung",   "W",    "power",    "measurement"},
    {"saldo",   "Saldo",                "W",    "power",    "measurement"},
};


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


// Helper: get a (valid) topic for the HA publishing
static String getHaTopic(MqttConfig_t &configMqtt)
{
    String avail_topic;
    if (!configMqtt.mqtt_pub.isEmpty()) {
        avail_topic = configMqtt.mqtt_pub;
        if (!avail_topic.endsWith("/")) {
            avail_topic += "/";
        }
        avail_topic += F("status");
    } else {
        avail_topic = sanitizeTopic(Config.DeviceName) + F("/status");
    }
    return avail_topic;
}


void MqttHAClass::init(MqttBaseClass *mqttBase)
{
    _mqttBase = mqttBase;
}


// Helper that accepts the whole HASensor and builds the discovery payload,
// including rest_var handling and value_template logic.
void MqttHAClass::getTopicPayloadSingleSensor(String &topic, String &payload, const HASensor &e, const String &dev, const String &state_topic, const String &availability_topic)
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
    /*
    {
        "name": "Amis-1 Bezug",
        "state_topic": "amis/out/",
        "availability_topic": "amis/out/status",
        "unit_of_measurement": "kWh",
        "value_template": "{{ (value_json[\"1.8.0\"] / 1000) }}",
        "unique_id": "amis_1_448932_1_8_0",
        "device": {
            "identifiers": [
                "Amis-1"
            ],
            "name": "Amis-1",
            "model": "Amis",
            "sw_version": "1.5.8"
        },
        "device_class": "energy",
        "state_class": "total_increasing"
    }
    */
    DynamicJsonDocument root(768);
    String name = String(Config.DeviceName) + " " + e.name;
    root[F("name")] = name;
    root[F("state_topic")] = state_topic;
    root[F("availability_topic")] = availability_topic;
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
    JsonObject device = root.createNestedObject("device");
    device[F("identifiers")][0] = Config.DeviceName;
    device[F("name")] = Config.DeviceName;
    device[F("model")] = APP_NAME;
    device[F("sw_version")] = APP_VERSION_STR;
    if (e.device_class && e.device_class[0]) {
        root[F("device_class")] = e.device_class;
    }
    if (e.state_class && e.state_class[0]) {
        root[F("state_class")] = e.state_class;
    }

    SERIALIZE_JSON_LOG(root, payload);
    topic = "homeassistant/sensor/" + dev + "/" + obj + "/config";
};


// Publish Home Assistant MQTT discovery messages for all sensors
void MqttHAClass::publishHaDiscovery()
{
    MqttConfig_t configMqtt = _mqttBase->getConfigMqtt();
    String dev = sanitizeTopic(Config.DeviceName + String("_") + String(ESP.getChipId(), HEX));

    String state_topic = configMqtt.mqtt_pub;
    String availability_topic = getHaTopic(configMqtt);
    for (size_t i=0; i < std::size(HASensors); i++) {
        String topic, payload;
        getTopicPayloadSingleSensor(topic, payload, HASensors[i], dev, state_topic, availability_topic);
        _mqttBase->publish(topic.c_str(), configMqtt.mqtt_qos, true, payload.c_str());
    }
}


// Publish Home Assistant MQTT availbility messages for the whole topic
void MqttHAClass::publishHaAvailability(bool isOnline)
{
    if (!_mqttBase->isConnected()) {
        return;
    }
    MqttConfig_t configMqtt = _mqttBase->getConfigMqtt();
    String avail_topic = getHaTopic(configMqtt);
    String payload = isOnline ? "online" : "offline";
    _mqttBase->publish(avail_topic.c_str(), configMqtt.mqtt_qos, true, payload.c_str());
}


/* vim:set ft=cpp ts=4 et: */
