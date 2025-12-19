#pragma once

#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>


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


class MqttClass {
public:
    MqttClass();
    void init();
    bool isConnected();
    void start();
    void stop();

    void networkOnStationModeGotIP(const WiFiEventStationModeGotIP& event);
    void networkOnStationModeDisconnected(const WiFiEventStationModeDisconnected& event);

    void reloadConfig();

    //const MqttConfig_t &getConfigMqtt();

private:
    AsyncMqttClient _client;
    Ticker _ticker;

    void onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void onConnect(bool sessionPresent);
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onPublish(uint16_t packetId);

    void publishState();
    void publishHaAvailability(bool isOnline);
    void publishHaDiscovery();
    void aliveTicker();

    void connect();

    bool loadConfigMqtt(MqttConfig_t &config);
    MqttConfig_t _config;

    int _reloadConfigState;
};

extern MqttClass Mqtt;

/* vim:set ts=4 et: */
