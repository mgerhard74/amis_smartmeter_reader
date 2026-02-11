// Handle publishing current counter data

#include "Mqtt.h"

#include "config.h"

#include <AsyncJson.h>


void MqttReaderDataClass::init(MqttBaseClass *mqttBase)
{
    _mqttBase = mqttBase;
}


 void MqttReaderDataClass::publish()
{
    MqttConfig_t config = _mqttBase->getConfigMqtt();

    StaticJsonDocument<384> root; // Keys NOT <const char*> // CHECK
    /*
    {
        "1.8.0": 4294967295,
        "2.8.0": 4294967294,
        "3.8.1": 4294967293,
        "4.8.1": 4294967292,
        "1.7.0": 4294967291,
        "2.7.0": 4294967290,
        "3.7.0": 4294967289,
        "4.7.0": 4294967288,
        "1.128.0": -2147483647,
        "saldo": -2147483646,
        "time": 4294967288,
        "serialnumber": "abcdefghijklmnopqrstuvwxyz789012"
    }
    */
    signed saldo = Databroker.results_u32[4] - Databroker.results_u32[5] - Config.rest_ofs;
    if (Config.rest_neg) {
        saldo =-saldo;
    }

    // Variablennamen mit Punkten (".") oder Underscore("_") aufbereiten
    root[Config_restValueKeys[Config.rest_var][0]] = Databroker.results_u32[0];
    root[Config_restValueKeys[Config.rest_var][1]] = Databroker.results_u32[1];
    root[Config_restValueKeys[Config.rest_var][2]] = Databroker.results_u32[2];
    root[Config_restValueKeys[Config.rest_var][3]] = Databroker.results_u32[3];
    root[Config_restValueKeys[Config.rest_var][4]] = Databroker.results_u32[4];
    root[Config_restValueKeys[Config.rest_var][5]] = Databroker.results_u32[5];
    root[Config_restValueKeys[Config.rest_var][6]] = Databroker.results_u32[6];
    root[Config_restValueKeys[Config.rest_var][7]] = Databroker.results_u32[7];
    root[Config_restValueKeys[Config.rest_var][8]] = Databroker.results_i32[0];

    root[F("saldo")] = saldo;
    root[F("time")] = static_cast<uint32_t>(Databroker.ts); // generate valid values till 2106
    root[F("serialnumber")] = AmisReader.getSerialNumber();

    String mqttBuffer;
    SERIALIZE_JSON_LOG(root, mqttBuffer);
    _mqttBase->publish(config.mqtt_pub.c_str(), config.mqtt_qos, config.mqtt_retain, mqttBuffer.c_str());
    /*
    // TODO(StefanOberhumer): Fix the json logging problem
    LOG_VP("publish(\"%s\", %" PRIu8 ", \"%s\", \"%s\")",
                    config.mqtt_pub.c_str(), config.mqtt_qos,
                    (config.mqtt_retain) ?"true" :"false",
                    Utils::escapeJson(mqttBuffer.c_str(), -1, -1).c_str());
    */
    LOGF_VP("publish(%s, %" PRIu8 ", %s, %s)",
                    config.mqtt_pub.c_str(), config.mqtt_qos,
                    (config.mqtt_retain) ?"true" :"false",
                    Utils::escapeJson(mqttBuffer.c_str(), -1, -1).c_str());

}

/* vim:set ft=cpp ts=4 et: */
