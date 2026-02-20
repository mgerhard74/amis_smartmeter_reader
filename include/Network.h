#pragma once

#include "RingBuffer.h"

#include <ESP8266WiFi.h>
#include <Ticker.h>

// Maximum of elements in /config_wifi (incl "command" and all other values)
#define NETWORK_JSON_CONFIG_WIFI_DOCUMENT_SIZE      JSON_OBJECT_SIZE(16) + 768

// Contains all the settings saved in file '/config_wifi'
typedef struct {
    bool allow_sleep_mode;

    char ssid[32+1];                // max uint8_t[32] - see WiFi.begin(ssid, password);
    char wifipassword[64+1];        // max uint8_t[64] - see WiFi.begin(ssid, password);
    int32_t channel;                // 2.4Ghz channels: 0(=auto) and 1 ... 13

    bool mdns;
    unsigned int rfpower;

    bool dhcp;
    IPAddress ip_static;
    IPAddress ip_netmask;
    IPAddress ip_gateway;
    IPAddress ip_nameserver;

    bool pingrestart_do;
    IPAddress pingrestart_ip;
    unsigned pingrestart_interval;
    unsigned pingrestart_max;
} NetworkConfigWifi_t;





class NetworkClass {
public:
    void init(bool apMode);
    void loop(void);
    void connect(void);
    bool inAPMode(void);
    bool isConnected(void);
    const NetworkConfigWifi_t &getConfigWifi();
    void restartMDNSIfNeeded();

private:
    typedef struct _networkEvent {
        unsigned event;
        WiFiEventStationModeGotIP eventGotIP;
        WiFiEventStationModeDisconnected eventDisconnected;
        _networkEvent()
            : event(0)
            , eventGotIP()
            , eventDisconnected()
            {}
    } _networkEvent_t;


    WiFiEventHandler _onStationModeGotIP;
    void onStationModeGotIPCb(const WiFiEventStationModeGotIP& event);
    void onStationModeGotIP(_networkEvent_t& nwevent);
    WiFiEventHandler _onStationModeDisconnected;
    void onStationModeDisconnectedCb(const WiFiEventStationModeDisconnected& event);
    void onStationModeDisconnected(_networkEvent_t& nwevent);

    bool loadConfigWifi(NetworkConfigWifi_t &config);
    bool loadConfigWifiFromEEPROM(NetworkConfigWifi_t &config);

    String getValidHostname(const char *hostname);

    Ticker _tickerReconnect;

    bool _isConnected;
    bool _apMode;

    NetworkConfigWifi_t _configWifi;

    RingBuffer<_networkEvent_t, 4, true> _networkEvents;
};

extern NetworkClass Network;

/* vim:set ts=4 et: */
