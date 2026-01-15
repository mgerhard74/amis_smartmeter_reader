#include "Mqtt.h"

#include "AmisReader.h"
#include "Application.h"
#include "config.h"
#include "DefaultConfigurations.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_MQTT
#include "Network.h"
#include "unused.h"
#include "Utils.h"

#include <AsyncJson.h>
#include <LittleFS.h>

//#define DEBUG
#include "debug.h"

#ifdef STROMPREIS
extern String strompreis;
#endif

#include "MqttReaderDataClass.inl"  // Handling "normal" data publishing        (topic = homeassistant/sensor/ .... )
#include "MqttHAClass.inl"          // Handling HomeAssistant data publishing   (topic = config.mqtt_pub)
#include "MqttBaseClass.inl"


/* vim:set ts=4 et: */
