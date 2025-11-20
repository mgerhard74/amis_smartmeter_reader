#include "proj.h"
#include "AmisReader.h"
#include "ModbusSmartmeterEmulation.h"

//#define DEBUG
#include "debug.h"

AsyncWebServer server(80);
//AsyncWebServer restserver(81);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
//flag to use from web update to reboot the ESP
bool shouldReboot = false;
int cmd;
File fsUploadFile;

void  onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
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

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    // the request handler is triggered after the upload has finished...
    AsyncWebServerResponse *response = request->beginResponse(200,F("text/html"),"");
    request->send(response);
    DBGOUT(F("on_update\n"));
  },
  [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    //Upload handler chunks in data
    if(!index){                                                    // Start der Übertragung: index==0
      if (Config.log_sys) writeEvent("INFO", "updt", "Update started", filename.c_str());
      eprintf("Update Start: %s\n", filename.c_str());
      uint32_t content_len=0;
      if (filename.startsWith(F("firmware"))) {
        cmd=U_FLASH;                       // 0
        content_len= (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        ModbusSmartmeterEmulation.disable();
        valid=6;
        Serial.end();
      }
      else if (filename==F("spiffs.bin")) {
        DBGOUT("SPIFFS is deprecated\n");
        ESP.restart();
      }
      else if (filename==F("littlefs.bin")) {
        cmd= U_FS;                         // 100
        content_len=((size_t) &_FS_end - (size_t) &_FS_start);        // eigentlich Größe d. Flash-Partition
      }
      else {
        cmd=1000;                       // anderes File
      }
      //eprintf("command: %d  content_len: %x  request: %x \n",cmd,content_len,request->contentLength());
      // SPIFFS-Partition: eprintf("_FS_start %x;  _FS_end %x; Size: %x\n",(size_t)&_FS_start,(size_t)&_FS_end,(size_t)&_FS_end-(size_t)&_FS_start);
      if (cmd < 1000) {
        Update.runAsync(true);
        if(!Update.begin(content_len, cmd)) {
            Update.printError(Serial);
            //return request->send(400, "text/plain", "OTA could not begin firmware");
        }
      }
      else {
        if(!filename.startsWith("/")) filename = "/"+filename;
        fsUploadFile = LittleFS.open(filename, "w");            // Open the file for writing in LittleFS (create if it doesn't exist)
        if (!fsUploadFile) DBGOUT(F("Err filecreate\n"));
      }
    }       // !index
    // jetzt kommen die Daten:
    if (cmd < 1000) {                 // Update Flash
      if(!Update.hasError()){
        if(Update.write(data, len) != len){
          if (Config.log_sys) writeEvent("ERRO", "updt", "Writing to flash has failed", filename.c_str());
          Update.printError(Serial);
        } //else DBGOUT(".");  // eprintf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
      }
    }
    else {                            // write File
      if(fsUploadFile) {
        int written=fsUploadFile.write(data,len); // Write the received bytes to the file
        eprintf("written: %u\n",written);
      }
      else DBGOUT(F("write err\n"));
    }
    if(final){                        // Ende Übertragung erreicht
      if (cmd < 1000) {               // Flash Update
        if(Update.end(true)){
          eprintf("Update Success: %uB\n", index+len);
          if (Config.log_sys) writeEvent("INFO", "updt", "Firmware update has finished", "");
        } else {
          if (Config.log_sys) writeEvent("ERRO", "updt", "Update has failed", "");
          Update.printError(Serial);
          //return request->send(400, "text/plain", "Could not end OTA");
        }
        shouldReboot = true;
      }
      else {                          // File write
        fsUploadFile.close();
        DBGOUT(F("File end\n"));
      }
    }
  });

  server.on("/upgrade", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200,F("text/html"), PAGE_upgrade);
  });

  server.on("/rest", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (amisNotOnline==false && valid==5) {
      AsyncResponseStream *response = request->beginResponseStream(F("application/json;charset=UTF-8"));
      DynamicJsonBuffer jsonBuffer;
      JsonObject &root = jsonBuffer.createObject();
      signed saldo= (a_result[4]-a_result[5]-Config.rest_ofs);
      if (Config.rest_neg) saldo =-saldo;
      if (Config.rest_var==0) {
        root[F("time")] = a_result[9];
        root[F("1.8.0")] = a_result[0];
        root[F("2.8.0")] = a_result[1];
        root[F("3.8.1")] = a_result[2];
        root[F("4.8.1")] = a_result[3];
        root[F("1.7.0")] = a_result[4];
        root[F("2.7.0")] = a_result[5];
        root[F("3.7.0")] = a_result[6];
        root[F("4.7.0")] = a_result[7];
        root[F("1.128.0")] = a_result[8];
        root[F("saldo")] = saldo;
      }
      else {
        root[F("time")] = a_result[9];
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
      }
      root[F("serialnumber")] = AmisReader.getSerialNumber();
      //root.prettyPrintTo(*response);
      root.printTo(*response);
      request->send(response);
    }
  });

  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
    String remoteIP = request->client()->remoteIP().toString();
    DBGOUT("login "+remoteIP+"\n");
    if (!Config.use_auth) {
      request->send(200, F("text/plain"), F("Success"));
      return;
    }
    if(!request->authenticate(Config.auth_user.c_str(), Config.auth_passwd.c_str())) {
      if (Config.log_sys) writeEvent("WARN", "websrv", "New login attempt", remoteIP);
      eprintf("login fail %s %s\n",Config.auth_user.c_str(), Config.auth_passwd.c_str());
      return request->requestAuthentication(Config.DeviceName.c_str());
    }
    request->send(200, F("text/plain"), F("Success"));
    DBGOUT(F("login ok\n"));
    if (Config.log_sys) writeEvent("INFO", "websrv", "Login success!", remoteIP);
  });

  //server.onNotFound([](AsyncWebServerRequest *request){request->send(404,"text/plain","404 not found!");});
  server.onNotFound([](AsyncWebServerRequest *request) {
  if (request->method() == HTTP_OPTIONS) {
    DBGOUT(F("HTTP-Options\n"));
    request->send(200);
  } else {
    request->send(404,F("text/plain"),F("404 not found!"));
  }
});
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
