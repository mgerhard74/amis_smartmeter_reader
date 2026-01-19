#pragma once

#include <ESPAsyncWebServer.h>

class WebserverUpdateClass
{
    public:
        void init(AsyncWebServer& server);

    private:
        void onUploadRequest(AsyncWebServerRequest* request);
        void onUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final);
        void onBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);

        File _uploadFile;
        typedef enum {
            firmware = U_FLASH,
            littlefs = U_FS,
            anyOther,
            none
        } uploadFileType_t;
        uploadFileType_t _uploadfiletype;
        String _uploadFilename;
};

/* vim:set ts=4 et: */
