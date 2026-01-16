#pragma once

#include <ESP8266WiFi.h>
#include <Ticker.h>

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
    void connect(void);
    bool inAPMode(void);
    bool isConnected(void);
    const NetworkConfigWifi_t &getConfigWifi();
    void restartMDNSIfNeeded();

private:
    WiFiEventHandler _onStationModeGotIP;
    void onStationModeGotIP(const WiFiEventStationModeGotIP& event);
    WiFiEventHandler _onStationModeDisconnected;
    void onStationModeDisconnected(const WiFiEventStationModeDisconnected& event);

    bool loadConfigWifi(NetworkConfigWifi_t &config);
    bool loadConfigWifiFromEEPROM(NetworkConfigWifi_t &config);

    String getHostname(const char *hostname);

    Ticker _tickerReconnect;

    bool _isConnected;
    bool _apMode;

    NetworkConfigWifi_t _configWifi;
};

extern NetworkClass Network;

/* vim:set ts=4 et: */
