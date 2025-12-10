#if 0
#include "proj.h"
#include "AmisReader.h"
#include "ModbusSmartmeterEmulation.h"
#include "Reboot.h"

//#define DEBUG
#include "debug.h"

AsyncWebServer server(80);
//AsyncWebServer restserver(81);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
//flag to use from web update to reboot the ESP
int cmd;
File fsUploadFile;

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  //eprintf("[ WARN ] WebSocket[%s][%u] type(%u)\n", server->url(), client->id(), type);
  if(type == WS_EVT_ERROR) {
    eprintf("[ WARN ] WebSocket[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if(type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    uint64_t index = info->index;
    uint64_t infolen = info->len;
    if(info->final && info->index == 0 && infolen == len) {
      //the whole message is in a single frame and we got all of it's data
      client->_tempObject = malloc(len+1);
      if(client->_tempObject != NULL) {
        memcpy((uint8_t *)(client->_tempObject), data, len);
      }
      //procMsg(client, infolen);
      ((uint8_t *)client->_tempObject)[len]=0;
      wsClientRequest(client, infolen);

    }
    else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(index == 0) {
        if(info->num == 0 && client->_tempObject == NULL) {
          client->_tempObject = malloc(infolen+1);
        }
      }
      if(client->_tempObject != NULL) {
        memcpy((uint8_t *)(client->_tempObject) + index, data, len);
      }
      if((index + len) == infolen) {
        if(info->final) {
          //procMsg(client, infolen);
          ((uint8_t *)client->_tempObject)[infolen]=0;
          wsClientRequest(client, infolen);
        }
      }
    }
  }
}

//*************************************************************

void serverInit(unsigned mode) {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  if (Config.use_auth) ws.setAuthentication(Config.auth_user.c_str(), Config.auth_passwd.c_str());
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");   //CORS-Header allgem.



  if (mode==0) server.rewrite("/", "/index.html");
  else 	server.rewrite("/", "/upgrade");
  // attach filesystem root at URL /fs
  // server.serveStatic("/fs", LittleFS, "/");
  server.serveStatic("/", LittleFS, "/", "public, must-revalidate");  // /*.* wird aut.  geservt, alle Files die keine Daten anfordern (GET,POST...)
  server.begin();
}
//*************************************************************

#ifdef OTA
void initOTA(){
  ArduinoOTA.onStart([]() {
    DBGOUT("OTA Start\n");
    WiFiUDP::stopAll();
    WiFiClient::stopAll();
    LittleFS.end();
    ws.closeAll();
    uniTicker.detach();
    secTicker.detach();
    #ifdef MQTT
    mqttClient.disconnect(true);
    mqttAliveTicker.detach();
    #endif // MQTT
  });
//    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
//      eprintf("Progress: %d%%\n", (progress*100)/total);
//    });
  ArduinoOTA.onError([](ota_error_t error) {
    #ifdef DEBUG
    eprintf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DBGOUT("Auth Failed\n");
    else if (error == OTA_BEGIN_ERROR) DBGOUT("Begin Failed\n");
    else if (error == OTA_CONNECT_ERROR) DBGOUT("Connect Failed\n");
    else if (error == OTA_RECEIVE_ERROR) DBGOUT("Receive Failed\n");
    else if (error == OTA_END_ERROR) DBGOUT("End Failed\n");
    #endif
    shouldReboot=true;
  });
  ArduinoOTA.onEnd([]() {
    DBGOUT("OTA End\n");
    shouldReboot=true;
  });
  //ArduinoOTA.setHostname(Config.host);
  ArduinoOTA.begin();
}
//*************************************************************
#endif // OTA
#endif
