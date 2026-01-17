// Handle publishing current counter data

#include "Mqtt.h"

#include "config.h"

#include <AsyncJson.h>


extern uint32_t a_result[10];
extern unsigned int first_frame;


void MqttReaderDataClass::init(MqttBaseClass *mqttBase)
{
    _mqttBase = mqttBase;
}


 void MqttReaderDataClass::publish()
{
    MqttConfig_t config = _mqttBase->getConfigMqtt();

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
        root[F("1.128.0")] = a_result[8];
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
