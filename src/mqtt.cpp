#include "proj.h"
#include "AmisReader.h"
#include "DefaultConfigurations.h"
#include "unused.h"

//#define DEBUG
#include "debug.h"

#ifdef STROMPREIS
extern String strompreis;
#endif

AsyncMqttClient mqttClient;
Ticker mqttTimer;

static bool loadConfigMqtt()
{
  File configFile = LittleFS.open("/config_mqtt", "r");
  if (!configFile) {
    DBGOUT(F("[ WARN ] Failed to open config_mqtt\n"));
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
    DBGOUT(F("[ WARN ] Failed to parse config_mqtt\n"));
    return false;
  }
  ///json.prettyPrintTo(Serial);
  Config.mqtt_qos = (*json)[F("mqtt_qos")].as<unsigned int>();
  Config.mqtt_retain = (*json)[F("mqtt_retain")].as<bool>();
  Config.mqtt_sub = (*json)[F("mqtt_sub")].as<String>();
  Config.mqtt_pub = (*json)[F("mqtt_pub")].as<String>();
  Config.mqtt_keep = (*json)[F("mqtt_keep")].as<unsigned int>();
  Config.mqtt_ha_discovery = (*json)[F("mqtt_ha_discovery")].as<bool>();
  Config.mqtt_will = (*json)[F("mqtt_will")].as<String>();
  Config.mqtt_user = (*json)[F("mqtt_user")].as<String>();
  Config.mqtt_password = (*json)[F("mqtt_password")].as<String>();
  Config.mqtt_client_id = (*json)[F("mqtt_clientid")].as<String>();
  Config.mqtt_enabled = (*json)[F("mqtt_enabled")].as<bool>();
  Config.mqtt_broker = (*json)[F("mqtt_broker")].as<String>();
  Config.mqtt_port = (*json)[F("mqtt_port")].as<uint16_t>();
  return true;
}

void mqttAliveTicker() {
  if (valid==5)  mqtt_publish_state();
}

void onMqttConnect(bool sessionPresent) {
  UNUSED_ARG(sessionPresent);
  mqttTimer.detach();
  if (Config.mqtt_keep) {
    mqttTimer.attach_scheduled(Config.mqtt_keep,mqttAliveTicker);
  }
  eprintf("MQTT onConnect %u %s\n",sessionPresent,Config.mqtt_sub.c_str());
  if (Config.log_sys) writeEvent("INFO", "mqtt", "Connected to MQTT Server", "Session Present");
  if (Config.mqtt_sub!="") {
    mqttClient.subscribe(Config.mqtt_sub.c_str(),Config.mqtt_qos);
    eprintf("MQTT subscr %s\n",Config.mqtt_sub.c_str());
  }
  mqttStatus=true;
  if (Config.mqtt_ha_discovery)
  {
    // Publish 'online' to availability topic (birth) so Home Assistant / other clients see device is online
    mqtt_publish_ha_availability(true);
    // Publish Home Assistant discovery info so HA can auto-detect this device (if enabled)
    mqtt_publish_ha_discovery();
  }
#ifdef STROMPREIS
  mqttClient.subscribe("strompreis",0);
#endif // STROMPREIS
}

void connectToMqtt() {
  if (!loadConfigMqtt()) {
    return;
  }

  if (!Config.mqtt_will.isEmpty()) {
    mqttClient.setWill(Config.mqtt_will.c_str(), Config.mqtt_qos, Config.mqtt_retain, Config.DeviceName.c_str());
    eprintf("MQTT SetWill: %s %u %u %s\n",Config.mqtt_will.c_str(), Config.mqtt_qos, Config.mqtt_retain, Config.DeviceName.c_str());
  } else if (Config.mqtt_ha_discovery) {
    // Set LWT to availability topic for HA discovery in case the user did not define a custom one
    String avail_topic = get_ha_availability_topic();
    // mqttClient.setWill(avail_topic.c_str(), Config.mqtt_qos, true, String("offline").c_str());
    eprintf("MQTT SetWill (HA): %s %u %u offline\n", avail_topic.c_str(), Config.mqtt_qos, true);
  }
  if (!Config.mqtt_user.isEmpty()) {
    mqttClient.setCredentials(Config.mqtt_user.c_str(), Config.mqtt_password.c_str());
    eprintf("MQTT User: %s %s\n", Config.mqtt_user.c_str(), Config.mqtt_password.c_str());
  }
  if (!Config.mqtt_client_id.isEmpty()) {
    mqttClient.setClientId(Config.mqtt_client_id.c_str());
    eprintf("MQTT ClientId: %s\n", Config.mqtt_client_id.c_str());
  }
  if (Config.mqtt_enabled) {
    DBGOUT("MQTT connect\n");
    mqttClient.connect();
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  mqttTimer.detach();
  String reasonstr = "";
  switch(reason) {
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
    default:
      reasonstr = F("Unknown");
      break;
  }
  if (Config.log_sys) writeEvent("WARN", "mqtt", "Disconnected from MQTT server", reasonstr);
  eprintf("Disconnected from MQTT server: %s\n", reasonstr.c_str());

  // If we have HA discovery enabled, try to publish offline status (clean disconnect)
  if (Config.mqtt_ha_discovery) mqtt_publish_ha_availability(false);
  //if(WiFi.isConnected()) {
    mqttTimer.attach_scheduled(15, connectToMqtt);
  //}
  mqttStatus=false;
}

void onMqttPublish(uint16_t packetId) {
// not working!!!
  DBGOUT ("onMqttPublish\n");
  if (Config.log_sys) writeEvent("INFO", "mqtt", "MQTT publish acknowledged", String(packetId));
}

void mqtt_publish_state() {
  if (mqttClient.connected() && first_frame==1) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    signed saldo = (a_result[4]-a_result[5]-Config.rest_ofs);
    if (Config.rest_neg) saldo =-saldo;
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
#ifdef STROMPREIS
      root[F("strompreis")]=strompreis;
#else
      root[F("1.128.0")] = a_result[8];
#endif
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
    mqttClient.publish(Config.mqtt_pub.c_str(),Config.mqtt_qos,Config.mqtt_retain,mqttBuffer.c_str());
    //DBGOUT("MQTT publish "+Config.mqtt_pub+": "+mqttBuffer+"\n");
  }
  else { DBGOUT("MQTT publish: not connected\n"); }
}


// Helper: sanitize a device name for topic/object id
static String sanitizeTopic(const String &s) {
  String t = s;
  for (unsigned int i = 0; i < t.length(); i++) {
    char c = t[i];
    if (!isalnum(c)) t[i] = '_';
    else t[i] = tolower(c);
  }
  return t;
}

String get_ha_availability_topic() {
  String avail_topic;
  if (Config.mqtt_pub.length()) {
    avail_topic = Config.mqtt_pub;
    if (!avail_topic.endsWith("/")) avail_topic += "/";
    avail_topic += "status";
  } else {
    avail_topic = sanitizeTopic(Config.DeviceName) + String("/status");
  }
  return avail_topic;
}

void mqtt_publish_ha_availability(bool isOnline) {
  if (!mqttClient.connected()) return;
  String avail_topic = get_ha_availability_topic();
  String payload = isOnline ? "online" : "offline";
  mqttClient.publish(avail_topic.c_str(), Config.mqtt_qos, true, payload.c_str());
}

// Publish Home Assistant MQTT discovery messages for all sensors
void mqtt_publish_ha_discovery() {

  if (!mqttClient.connected()) return;
  // Use the configured state topic as the main state_topic for sensors
  String state_topic = Config.mqtt_pub;
  if (state_topic.length() == 0) return;
  String chipId = String(ESP.getChipId(), HEX);
  String dev = sanitizeTopic(Config.DeviceName + String("_") + chipId);
  // Publish discovery for all relevant measurement keys. Use bracket notation
  // for JSON access to handle keys with dots or underscores.
  struct HASensor { const char *key; const char *name; const char *unit; const char *device_class; const char *state_class; };
  HASensor entries[] = {
    {"1.8.0","Bezug","kWh","energy","total_increasing"},
    {"2.8.0","Lieferung","kWh","energy","total_increasing"},
    {"1.7.0","Leistung Bezug","W","power","measurement"},
    {"2.7.0","Leistung Lieferung","W","power","measurement"},
    {"saldo","Saldo","W", "power","measurement"},
  };
  // Helper that accepts the whole HASensor and builds the discovery payload,
  // including rest_var handling and value_template logic.
  auto publishSensor = [&](const HASensor &e){
    // choose key variant depending on rest_var (dots vs underscores)
  String key_use = String(e.key);
  if (Config.rest_var != 0) key_use.replace('.', '_');

    // Build the value_template: energy entries are in Wh -> convert to kWh (/1000)
    String tpl;
    if (e.device_class && strcmp(e.device_class, "energy") == 0) {
      tpl = String("{{ (value_json[\"") + key_use + String("\"] / 1000) }}");
    } else {
      tpl = String("{{ value_json[\"") + key_use + String("\"] }}");
    }

    // Build the discovery JSON
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    String name = String(Config.DeviceName) + " " + e.name;
    root[F("name")] = name;
    root[F("state_topic")] = state_topic;
    root[F("availability_topic")] = get_ha_availability_topic();
    // root[F("payload_available")] = "online";
    // root[F("payload_not_available")] = "offline";

    if (e.unit && strlen(e.unit)) root[F("unit_of_measurement")] = e.unit;
    if (tpl.length()) root[F("value_template")] = tpl;
    String obj = sanitizeTopic(key_use);
    String uid = dev + String("_") + obj;
    root[F("unique_id")] = uid;
    // attach device object
    JsonObject &devn = root.createNestedObject("device");
    JsonArray &ids = devn.createNestedArray("identifiers");
    ids.add(Config.DeviceName);
    devn[F("name")] = Config.DeviceName;
    devn[F("model")] = APP_NAME;
    devn[F("sw_version")] = VERSION;
  if (e.device_class && strlen(e.device_class)) root[F("device_class")] = e.device_class;
  if (e.state_class && strlen(e.state_class)) root[F("state_class")] = e.state_class;

    String out;
    root.printTo(out);
    String topic = String("homeassistant/sensor/") + dev + String("/") + obj + String("/config");
    mqttClient.publish(topic.c_str(), Config.mqtt_qos, true, out.c_str());
  };

  for (unsigned i=0;i<sizeof(entries)/sizeof(entries[0]);i++){
    publishSensor(entries[i]);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  UNUSED_ARG(topic);
  UNUSED_ARG(payload);
  UNUSED_ARG(properties);
  UNUSED_ARG(len);
  UNUSED_ARG(index);
  UNUSED_ARG(total);
#ifdef STROMPREIS
  char p[20];
  memcpy(p,payload,len);
  p[len]=0;
  strompreis = String(p);
  //DBGOUT(strompreis+"\n");
#endif // STROMPREIS
}

void mqtt_init() {
  if (!loadConfigMqtt()) {
    return;
  }

  if (Config.mqtt_enabled) {
//    mqttClient.onSubscribe(onMqttSubscribe);
//    mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onPublish(onMqttPublish);
    IPAddress mqttIP;
    mqttIP.fromString(Config.mqtt_broker);
    eprintf("MQTT init: %s %d\n", mqttIP.toString().c_str(), Config.mqtt_port);
    mqttClient.setServer(mqttIP, Config.mqtt_port);
    mqttTimer.once_scheduled(15, connectToMqtt);
  }
}
