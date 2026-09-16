/*
    Handle updates of firmware or any other fileupload (POST requests)
    at http://<espiIp>/update

    Configuration changes get updated via the websochet and a command
*/
#include "Webserver_Update.h"

#include "AmisReader.h"
#include "config.h"
#include "Log.h"
#define LOGMODULE LOGMODULE_UPDATE
#include "Reboot.h"
#include "SystemMonitor.h"
#include "unused.h"
#include "Webserver.h"

#include <LittleFS.h>


extern void historyInit(void);


void WebserverUpdateClass::init(AsyncWebServer& server)
{
     _uploadfiletype = none;

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    server.on("/update", HTTP_POST,
                std::bind(&WebserverUpdateClass::onUploadRequest, this, _1),
                std::bind(&WebserverUpdateClass::onUpload, this, _1, _2, _3, _4, _5, _6));
}


void WebserverUpdateClass::onUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final)
{
#if 1
    if (!Webserver.checkCredentials(request, false)) {
        // skip any update if credentials do not match
        return;
    }
#else
    if (!Webserver.checkCredentials(request, true)) { // return a 401 here
        // We should add header "Connection: close" in this case to stop upload immediately
        // But: request->requestAuthentication(); does not support adding headers
        request->client()->stop();
        return;
    }
#endif

    //Upload handler chunks in data
    if (!index) {  // Start der Übertragung: index==0
        LOGF_IP("Update started: %s", filename.c_str());
        if (filename.isEmpty()) {
            return;
        }
        _uploadFilename = filename;

        size_t content_len = 0;

        if (_uploadFilename.startsWith(F("firmware"))) {
            if (!Reboot.startUpdateFirmware()) {
                return;
            }
            _uploadfiletype = firmware;
            content_len = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        } else {
            _uploadfiletype = anyOther;// anderes File
        }

        //eprintf("command: %d  content_len: %x  request: %x \n",cmd,content_len,request->contentLength());
        // SPIFFS-Partition: eprintf("_FS_start %x;  _FS_end %x; Size: %x\n",(size_t)&_FS_start,(size_t)&_FS_end,(size_t)&_FS_end-(size_t)&_FS_start);
        if (_uploadfiletype == firmware) {
            Update.runAsync(true);
            if (!Update.begin(content_len, _uploadfiletype)) {
                Update.printError(Serial);
                //return request->send(400, "text/plain", "OTA could not begin firmware");
            }
        } else {
            if (!_uploadFilename.startsWith("/")) {
                _uploadFilename = "/" + _uploadFilename;
            }
            if (_uploadFilename.equals(F("/monate"))) {
                _uploadfiletype = monate;
                AmisReader.disable();
            }
            _uploadFile = LittleFS.open(_uploadFilename, "w");// Open the file for writing in LittleFS (create if it doesn't exist)
            if (!_uploadFile) {
                LOGF_EP("Error creating file: %s", _uploadFilename.c_str());
            }
        }
    }       // !index


    // jetzt kommen die Daten:
    if (_uploadfiletype == firmware) { // Update Flash
        if (!Update.hasError()) {
            if (Update.write(data, len) != len) {
                LOGF_EP("Error writing to flash: %s", _uploadFilename.c_str());
                Update.printError(Serial);
            }
        }
    } else { // just write "any other file" content to file system
        if (_uploadFile) {
            if (_uploadFile.write(data, len) != len) {
                LOGF_EP("Error writing file: %s", _uploadFilename.c_str());
            }
        }
    }

    if (final) {  // Ende Übertragung erreicht
        if (_uploadfiletype == firmware) {
            // Flash oder LittleFS Update
            if (Update.end(true)) {
                LOG_IP("Update success.");
            } else {
                LOG_EP("Update failed");
                Update.printError(Serial);
                //return request->send(400, "text/plain", "Could not end OTA");
            }
            Reboot.endUpdateFirmware();
        } else {                          // File write
            if (_uploadFile) {
                _uploadFile.close();
            }
            if (_uploadfiletype == monate) {
                historyInit();
                AmisReader.enable();
            }
        }
        _uploadfiletype = none;
    }
    SYSTEMMONITOR_STAT();
}

void WebserverUpdateClass::onUploadRequest(AsyncWebServerRequest* request)
{
    // The request handler triggers once the upload has finished and should return an appropriate HTTP status.
    if (Webserver.checkCredentials(request, true)) {
        AsyncWebServerResponse *response = request->beginResponse(200, F("text/html"), "Upload finished");
        request->send(response);
    }
    LOG_DP("WebserverUpdateClass::onUploadRequest()");
}

/* vim:set ts=4 et: */
