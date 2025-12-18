#pragma once

#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>

class MqttClass {
public:
    MqttClass();
    void init();
    bool isConnected();
    bool loadConfigMqtt();
    void stop();

    void networkOnStationModeGotIP(const WiFiEventStationModeGotIP& event);
    void networkOnStationModeDisconnected(const WiFiEventStationModeDisconnected& event);

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
};

extern MqttClass Mqtt;

/* vim:set ts=4 et: */
