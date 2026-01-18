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

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    signed saldo = Databroker.results_u32[4] - Databroker.results_u32[5] - Config.rest_ofs;
    if (Config.rest_neg) {
        saldo =-saldo;
    }
    if (Config.rest_var == 0) {
        // Variablennamen mit Punkten (".")
        root[F("1.8.0")] = Databroker.results_u32[0];
        root[F("2.8.0")] = Databroker.results_u32[1];
        root[F("3.8.1")] = Databroker.results_u32[2];
        root[F("4.8.1")] = Databroker.results_u32[3];
        root[F("1.7.0")] = Databroker.results_u32[4];
        root[F("2.7.0")] = Databroker.results_u32[5];
        root[F("3.7.0")] = Databroker.results_u32[6];
        root[F("4.7.0")] = Databroker.results_u32[7];
        root[F("1.128.0")] = Databroker.results_i32[0];
    } else {
        // Variablennamen mit Unterstrichen ("_")
        root[F("1_8_0")] = Databroker.results_u32[0];
        root[F("2_8_0")] = Databroker.results_u32[1];
        root[F("3_8_1")] = Databroker.results_u32[2];
        root[F("4_8_1")] = Databroker.results_u32[3];
        root[F("1_7_0")] = Databroker.results_u32[4];
        root[F("2_7_0")] = Databroker.results_u32[5];
        root[F("3_7_0")] = Databroker.results_u32[6];
        root[F("4_7_0")] = Databroker.results_u32[7];
        root[F("1_128_0")] = Databroker.results_i32[0];
    }
    root[F("saldo")] = saldo;
    root[F("time")] = Databroker.ts;
    root[F("serialnumber")] = AmisReader.getSerialNumber();

    String mqttBuffer;
    //root.prettyPrintTo(mqttBuffer);
    root.printTo(mqttBuffer);
    jsonBuffer.clear();
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
