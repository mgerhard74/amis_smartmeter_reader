#include "Mqtt.h"

#include "AmisReader.h"
#include "config.h"
#include "DefaultConfigurations.h"
#include "Network.h"
#include "unused.h"

#include <AsyncJson.h>
#include <LittleFS.h>

//#define DEBUG
#include "debug.h"

#ifdef STROMPREIS
extern String strompreis;
#endif


extern void writeEvent(String, String, String, String);


#include "MqttReaderDataClass.inl"  // Handling "normal" data publishing        (topic = homeassistant/sensor/ .... )
#include "MqttHAClass.inl"          // Handling HomeAssistant data publishing   (topic = config.mqtt_pub)
#include "MqttBaseClass.inl"


/* vim:set ts=4 et: */
