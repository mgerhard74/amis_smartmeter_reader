#pragma once

#include <ESPAsyncWebServer.h>

class WebserverUpdateClass
{
    public:
        void init(AsyncWebServer& server);

    private:
        void onUploadRequest(AsyncWebServerRequest* request);
        void onUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final);

        File _uploadFile;
        typedef enum {
            firmware = U_FLASH,
            monate,
            // tageswerte,      ... must be reloaded after upload
            // config_general,  ... must be reloaded after upload
            // config_mqtt,     ... must be reloaded after upload
            // config_wifi,     ... must be reloaded after upload
            anyOther,
            none
        } uploadFileType_t;
        uploadFileType_t _uploadfiletype;
        String _uploadFilename;
};

/* vim:set ts=4 et: */
