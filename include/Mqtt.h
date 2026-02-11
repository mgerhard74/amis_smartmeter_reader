#pragma once

#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>


// Maximum of elements in /config_mqtt (incl "command" and all other values)
#define MQTT_JSON_CONFIG_MQTT_DOCUMENT_SIZE    JSON_OBJECT_SIZE(13) + 768

// Contains all the settings saved in file '/config_mqtt'
typedef struct {
    bool mqtt_retain;
    uint8_t mqtt_qos;
    unsigned mqtt_keep;
    String mqtt_pub;
    //String mqtt_sub; // mqtt_sub is currently not configurable
    bool mqtt_ha_discovery;
    String mqtt_will;
    String mqtt_user;
    String mqtt_password;
    String mqtt_client_id;
    bool mqtt_enabled;
    String mqtt_broker;
    uint16_t mqtt_port;
} MqttConfig_t;


// some forward declarations
class MqttBaseClass;
struct HASensor;


class MqttHAClass
{
public:
    void init(MqttBaseClass *mqttBase);
    void publishHaDiscovery();
    void publishHaAvailability(bool isOnline);
private:
    void getTopicPayloadSingleSensor(String &topic, String &paylod, const HASensor &e, const String &dev, const String &state_topic, const String &availability_topic);
    MqttBaseClass *_mqttBase;
};

class MqttReaderDataClass
{
public:
    void init(MqttBaseClass *mqttBase);
    void publish();
private:
    MqttBaseClass *_mqttBase;
};

class MqttBaseClass
{
public:
    MqttBaseClass();
    void init();
    void stop();
    bool isConnected();

    void networkOnStationModeGotIP(const WiFiEventStationModeGotIP& event);
    void networkOnStationModeDisconnected(const WiFiEventStationModeDisconnected& event);

    void reloadConfig();

    uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload);

    const MqttConfig_t &getConfigMqtt();

private:
    AsyncMqttClient _mqttClient;
    Ticker _reconnectTicker;
    Ticker _actionTicker;

    void onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void onConnect(bool sessionPresent);
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onPublish(uint16_t packetId);

    void publishTickerCb();

    void doConnect();

    bool loadConfigMqtt(MqttConfig_t &config);
    MqttConfig_t _config;
    IPAddress _brokerIp;
    bool _brokerByIPAddr;

    int _reloadConfigState;

    MqttReaderDataClass _mqttReaderData;
    MqttHAClass         _mqttHA;
};

extern MqttBaseClass Mqtt;

/* vim:set ts=4 et: */
