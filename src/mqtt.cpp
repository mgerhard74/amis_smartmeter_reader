#include "proj.h"
//#define DEBUG
#ifndef DEBUG
  #define eprintf( fmt, args... )
  #define DBGOUT(...)
#else
  #if DEBUGHW>0
    #define FOO(...) __VA_ARGS__
    #define DBGOUT dbg_string+= FOO
    #if (DEBUGHW==2)
      #define eprintf(fmt, args...) S.printf(fmt, ##args)
    #elif (DEBUGHW==1 || DEBUGHW==3)
      #define eprintf(fmt, args...) {sprintf(dbg,fmt, ##args);dbg_string+=dbg;dbg[0]=0;}
    #endif
  #else
    #define eprintf( fmt, args... )
    #define DBGOUT(...)
  #endif
#endif

#ifdef STROMPREIS
extern String strompreis;
#endif

AsyncMqttClient mqttClient;
Ticker mqttTimer;

void mqttAliveTicker() {
  if (valid==5)  mqtt_publish_state();
}

void onMqttConnect(bool sessionPresent) {
  mqttTimer.detach();
  if (config.mqtt_keep) {
    mqttTimer.attach_scheduled(config.mqtt_keep,mqttAliveTicker);
  }
  eprintf("MQTT onConnect %u %s\n",sessionPresent,config.mqtt_sub.c_str());
  if (config.log_sys) writeEvent("INFO", "mqtt", "Connected to MQTT Server", "Session Present");
  if (config.mqtt_sub!="") {
    mqttClient.subscribe(config.mqtt_sub.c_str(),config.mqtt_qos);
    eprintf("MQTT subscr %s\n",config.mqtt_sub.c_str());
  }
  mqttStatus=true;
#ifdef STROMPREIS
  mqttClient.subscribe("strompreis",0);
#endif // STROMPREIS

}

void connectToMqtt() {
  File configFile = LittleFS.open("/config_mqtt", "r");
  if(!configFile) {
    DBGOUT(F("[ WARN ] Failed to open config_mqtt\n"));
    return;
  }
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(configFile);
  configFile.close();
  if(!json.success()) {
    DBGOUT(F("[ WARN ] Failed to parse config_mqtt\n"));
    return;
  }
  ///json.prettyPrintTo(Serial);
  config.mqtt_qos=json[F("mqtt_qos")];
  config.mqtt_retain=json[F("mqtt_retain")];
  config.mqtt_sub=json[F("mqtt_sub")].as<String>();
  config.mqtt_pub=json[F("mqtt_pub")].as<String>();
  config.mqtt_keep=json[F("mqtt_keep")].as<int>();
  if (json[F("mqtt_will")]!="") {
    mqttClient.setWill(json[F("mqtt_will")].as<char*>(),config.mqtt_qos,config.mqtt_retain,config.DeviceName.c_str());
    eprintf("MQTT SetWill: %s %u %u %s\n",json[F("mqtt_will")].as<char*>(),config.mqtt_qos,config.mqtt_retain,config.DeviceName.c_str());
  }
  if (json[F("mqtt_user")]!="") {
    mqttClient.setCredentials(json[F("mqtt_user")].as<char*>(),json[F("mqtt_password")].as<char*>());
    eprintf("MQTT User: %s %s\n",json[F("mqtt_user")].as<char*>(),json[F("mqtt_password")].as<char*>());
  }
  if (json[F("mqtt_clientid")]!="") {
    mqttClient.setClientId(json[F("mqtt_clientid")].as<char*>());
    eprintf("MQTT ClientId: %s\n",json[F("mqtt_clientid")].as<char*>());
  }
  if (json[F("mqtt_enabled")]) {
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
  if (config.log_sys) writeEvent("WARN", "mqtt", "Disconnected from MQTT server", reasonstr);
  eprintf("Disconnected from MQTT server: %s\n", reasonstr.c_str());

  //if(WiFi.isConnected()) {
    mqttTimer.attach_scheduled(15, connectToMqtt);
  //}
  mqttStatus=false;
}

void onMqttPublish(uint16_t packetId) {
// not working!!!
  DBGOUT ("onMqttPublish\n");
  if (config.log_sys) writeEvent("INFO", "mqtt", "MQTT publish acknowledged", String(packetId));
}

void mqtt_publish_state() {
  if (mqttClient.connected() && first_frame==1) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    signed saldo= (a_result[4]-a_result[5]-config.rest_ofs);
    if (config.rest_neg) saldo =-saldo;
    if (config.rest_var==0) {
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
      root[F("saldo")] = saldo;
	  root[F("time")] = a_result[9];
    }
    else {
      root[F("1_8_0")] = a_result[0];
      root[F("2_8_0")] = a_result[1];
      root[F("3_8_1")] = a_result[2];
      root[F("4_8_1")] = a_result[3];
      root[F("1_7_0")] = a_result[4];
      root[F("2_7_0")] = a_result[5];
      root[F("3_7_0")] = a_result[6];
      root[F("4_7_0")] = a_result[7];
      root[F("1_128_0")] = a_result[8];
      root[F("saldo")] = saldo;
	  root[F("time")] = a_result[9];
    }
    String mqttBuffer;
    //root.prettyPrintTo(mqttBuffer);
    root.printTo(mqttBuffer);
    mqttClient.publish(config.mqtt_pub.c_str(),config.mqtt_qos,config.mqtt_retain,mqttBuffer.c_str());
    //DBGOUT("MQTT publish "+config.mqtt_pub+": "+mqttBuffer+"\n");
  }
  else DBGOUT("MQTT publish: not connected\n");
}

// Hier sind keine IO-Zugriffe zulässig!
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
#ifdef STROMPREIS
  char p[20];
  memcpy(p,payload,len);
  p[len]=0;
  strompreis = String(p);
  //DBGOUT(strompreis+"\n");
#endif // STROMPREIS
}

void mqtt_init() {
  File configFile = LittleFS.open("/config_mqtt", "r");
  if(!configFile) {
    DBGOUT(F("[ WARN ] Failed to open config_mqtt\n"));
    writeEvent("ERROR", "mqtt", "MQTT config fail", "");
    return;
  }
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(configFile);
  configFile.close();
  if(!json.success()) {
    DBGOUT(F("[ WARN ] Failed to parse config_mqtt\n"));
    writeEvent("ERROR", "mqtt", "MQTT config error", "");
    return;
  }
  //json.prettyPrintTo(Serial);
  if (json[F("mqtt_enabled")]) {
//    mqttClient.onSubscribe(onMqttSubscribe);
//    mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onPublish(onMqttPublish);
    IPAddress mqttIP;
    mqttIP.fromString(json[F("mqtt_broker")].as<String>());
    eprintf("MQTT init: %s %d\n",mqttIP.toString().c_str(),json["mqtt_port"].as<int>());
    mqttClient.setServer(mqttIP,json[F("mqtt_port")]);
    mqttTimer.once_scheduled(15, connectToMqtt);
  }
}
