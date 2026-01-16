// Basic client & functions for MQTT
// Details done in
//    *  MqttHAClass            handling
//    *  MqttReaderDataClass

void MqttBaseClass::init()
{
    _mqttReaderData.init(*this);
    _mqttHA.init(*this);
    loadConfigMqtt(_config);
    _reloadConfigState = 0;
}

MqttBaseClass::MqttBaseClass()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    _mqttClient.onConnect(std::bind(&MqttBaseClass::onConnect, this, _1));
    _mqttClient.onDisconnect(std::bind(&MqttBaseClass::onDisconnect, this, _1));
    _mqttClient.onMessage(std::bind(&MqttBaseClass::onMessage, this, _1, _2, _3, _4, _5, _6));
    _mqttClient.onPublish(std::bind(&MqttBaseClass::onPublish, this, _1));
}


void MqttBaseClass::onPublish(uint16_t packetId) {
    // seems that callback is not not working!!!
    LOGF_WP("Unexpected onPublish() call: %d", packetId);
}


void MqttBaseClass::onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    // Currently AmisRead does not subscribe to any topic
    // So this should be never get called !
    UNUSED_ARG(topic);
    UNUSED_ARG(payload);
    UNUSED_ARG(properties);
    UNUSED_ARG(len);
    UNUSED_ARG(index);
    UNUSED_ARG(total);

    LOGF_WP("Unexpected onMessage() call: %s", topic);
#ifdef STROMPREIS
    char p[20];
    memcpy(p,payload,len);
    p[len]=0;
    strompreis = String(p);
    //DBGOUT(strompreis+"\n");
#endif // STROMPREIS
}

void MqttBaseClass::publishTickerCb() {
    _actionTicker.detach();

    if (!_mqttClient.connected()) {
        LOG_WP("publishTickerCb() but not connected!");
        return; // _actionTicker will be armed in onConnect()
    }

    if (valid == 5 && first_frame == 1) {
        LOG_DP("Publishing reader data");
        _mqttReaderData.publish();
        _actionTicker.once_scheduled(_config.mqtt_keep, std::bind(&MqttBaseClass::publishTickerCb, this));
    } else {
        // Try sending agin in 2 secs
        _actionTicker.once_scheduled(2, std::bind(&MqttBaseClass::publishTickerCb, this));
    }
}


void MqttBaseClass::onConnect(bool sessionPresent)
{
    UNUSED_ARG(sessionPresent);

    _reconnectTicker.detach();
    _actionTicker.detach();
    if (!_config.mqtt_enabled) {
        return; // hierher sollten wir eigentlich nie kommen
    }
    _actionTicker.once_scheduled(2, std::bind(&MqttBaseClass::publishTickerCb, this));

    LOGF_IP("Connected to server " PRsIP ":%" PRIu16, PRIPVal(_brokerIp), _config.mqtt_port);

    // Für HA melden wir uns mal "Online" und "verbreiten" alle unsere Sensoren
    if (_config.mqtt_ha_discovery) {
        _mqttHA.publishHaDiscovery();
        _mqttHA.publishHaAvailability(true);
    }
}


void MqttBaseClass::doConnect()
{
    if (!_config.mqtt_enabled) {
        return;
    }
    if (Network.inAPMode()) {
        return;
    }

    if (!_brokerIp.isSet()) {
        // WiFi.hostByName() is a "blocking call" with a default timeout of 10000ms (that raises watchdog!)
        // So we set timeout to 1000ms here (which should be enough for DNS lookup / also 1000ms used in HttpClient)
        // If the FQN is a "local" name, it seems, this does not work proper on the 8266

        IPAddress ip;
        if (!WiFi.hostByName(_config.mqtt_broker.c_str(), ip, 1000) || !ip.isSet()) {
            LOGF_EP("Could not get IPNumber for '%s'.", _config.mqtt_broker.c_str());
            _reconnectTicker.once_scheduled(5, std::bind(&MqttBaseClass::doConnect, this));
            return;
        }
        _brokerIp[0] = ip[0];
        _brokerIp[1] = ip[1];
        _brokerIp[2] = ip[2];
        _brokerIp[3] = ip[3];
        _brokerByIPAddr = false;
    }

    LOGF_DP("setServer: " PRsIP ":%" PRIu16, PRIPVal(_brokerIp), _config.mqtt_port);
    _mqttClient.setServer(_brokerIp, _config.mqtt_port);

    if (!_config.mqtt_will.isEmpty()) {
        _mqttClient.setWill(_config.mqtt_will.c_str(), _config.mqtt_qos, _config.mqtt_retain, Config.DeviceName.c_str());
        LOGF_DP("setWill: %s %u %u %s", _config.mqtt_will.c_str(), _config.mqtt_qos, _config.mqtt_retain, Config.DeviceName.c_str());
    }
    if (!_config.mqtt_user.isEmpty()) {
        _mqttClient.setCredentials(_config.mqtt_user.c_str(), _config.mqtt_password.c_str());
        LOGF_VP("user: '%s' password: '%s'", _config.mqtt_user.c_str(), _config.mqtt_password.c_str());
    }
    if (!_config.mqtt_client_id.isEmpty()) {
        _mqttClient.setClientId(_config.mqtt_client_id.c_str());
        LOGF_DP("setClientId: %s", _config.mqtt_client_id.c_str());
    }

    if (_brokerByIPAddr) {
        LOGF_DP("Connecting to server %s:%" PRIu16 "...", _config.mqtt_broker.c_str(), _config.mqtt_port);
    } else {
        LOGF_DP("Connecting to server %s:%" PRIu16 " [" PRsIP ":%d]...",
                _config.mqtt_broker.c_str(), _config.mqtt_port,
                PRIPVal(_brokerIp), _config.mqtt_port);
    }
    _mqttClient.connect();
}


void MqttBaseClass::onDisconnect(AsyncMqttClientDisconnectReason reason) {
    if (_reloadConfigState == 0) {
        _actionTicker.detach();
    }
    _reconnectTicker.detach();

    if (!_config.mqtt_enabled) {
        return;
    }

    // If we have HA discovery enabled, try to publish offline status (clean disconnect)
    if (_config.mqtt_ha_discovery) {
        _mqttHA.publishHaAvailability(false);
    }

    if (_reloadConfigState == 0 && Network.isConnected()) {
        _reconnectTicker.once_scheduled(2, std::bind(&MqttBaseClass::doConnect, this));
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
    case(AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT):
        reasonstr = F("TLS_BAD_FINGERPRINT");
        break;
    default:
        reasonstr = F("Unknown");
        break;
    }
    LOGF_WP("Disconnected from server " PRsIP ":%" PRIu16 " reason=%s", PRIPVal(_brokerIp), _config.mqtt_port, reasonstr.c_str());
}


void MqttBaseClass::networkOnStationModeGotIP(const WiFiEventStationModeGotIP& event)
{
    UNUSED_ARG(event);

    doConnect();
}


void MqttBaseClass::networkOnStationModeDisconnected(const WiFiEventStationModeDisconnected& event)
{
    UNUSED_ARG(event);

    _actionTicker.detach();
    _reconnectTicker.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    _mqttClient.disconnect(true);
}


void MqttBaseClass::reloadConfig() {
    if (_reloadConfigState == 0) {
        // Start disconnect
        _mqttClient.disconnect();
        _reloadConfigState = 1;
        _actionTicker.attach_ms(500, std::bind(&MqttBaseClass::reloadConfig, this));
    } else if (_reloadConfigState == 1) {
        // Wait till disconnect finisehd
        if (_mqttClient.connected()) {
            return;
        }
        _reloadConfigState = 2;
    } else if (_reloadConfigState == 2) {
        // disconnection finished --> reload configuration
        loadConfigMqtt(_config);
        _reloadConfigState = 3;
        LOG_IP("Config reloaded.");
    }  else if (_reloadConfigState == 3) {
        // finished ... try reconnecting if enabled
        _actionTicker.detach();
        _reloadConfigState = 0;
        if (Network.isConnected()) {
            doConnect();
        }
    }
}


const MqttConfig_t &MqttBaseClass::getConfigMqtt(void)
{
    return _config;
}


bool MqttBaseClass::loadConfigMqtt(MqttConfig_t &config)
{
    if (Application.inAPMode()) {
        // even skip loading any json in AP Mode (so we should not be able bricking the device)
        return false;
    }

    File configFile;
    configFile = LittleFS.open("/config_mqtt", "r");
    if (!configFile) {
        LOGF_EP("Could not open %s", "/config_mqtt");
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
        LOGF_EP("Failed parsing %s", "/config_mqtt");
        return false;
    }
    ///json.prettyPrintTo(Serial);
    config.mqtt_qos = (*json)[F("mqtt_qos")].as<uint8_t>();
    config.mqtt_retain = (*json)[F("mqtt_retain")].as<bool>();
#if 0 // mqtt_sub is currently not configurable
    config.mqtt_sub = (*json)[F("mqtt_sub")].as<String>();
#endif
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

    IPAddress ip;
    ip.fromString(config.mqtt_broker);
    _brokerIp[0] = ip[0];
    _brokerIp[1] = ip[1];
    _brokerIp[2] = ip[2];
    _brokerIp[3] = ip[3];
    _brokerByIPAddr = ip.isSet();

    return true;
}


bool MqttBaseClass::isConnected()
{
    return _mqttClient.connected();
}

uint16_t MqttBaseClass::publish(const char* topic, uint8_t qos, bool retain, const char* payload)
{
    return _mqttClient.publish(topic, qos, retain, payload);
}


void MqttBaseClass::stop()
{
    _mqttClient.disconnect();
    for (;isConnected();) {} // wait till disconnected
    _actionTicker.detach();
    _reconnectTicker.detach();
}

MqttBaseClass Mqtt;

/* vim:set ft=cpp ts=4 et: */
