#pragma once

#if (AMIS_BUILTIN_CONFIGURATION) && __has_include("local/DefaultConfigurations.cpp")

#define DEFAULT_CONFIG_GENERAL_JSON   defaultConfigGerneralJson
#define DEFAULT_CONFIG_WIFI_JSON      defaultConfigWifiJson
#define DEFAULT_CONFIG_MQTT_JSON      defaultConfigMqttJson

#endif

#ifdef DEFAULT_CONFIG_GENERAL_JSON
extern const char *defaultConfigGerneralJson;
#endif
#ifdef DEFAULT_CONFIG_WIFI_JSON
extern const char *defaultConfigWifiJson;
#endif
#ifdef DEFAULT_CONFIG_MQTT_JSON
extern const char *defaultConfigMqttJson;
#endif


/* vim:set ts=4 et: */
