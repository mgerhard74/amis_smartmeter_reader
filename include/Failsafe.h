#pragma once
//DO NOT include project-specific headers here, as this should be as standalone as possible 
#include "ESPAsyncWebServer.h"

#ifndef FAILSAFE_AP_SSID
#define FAILSAFE_AP_SSID "AMIS_FAILSAFE"
#endif

#ifndef FAILSAFE_AP_PASS
#define FAILSAFE_AP_PASS ""
#endif

#ifndef FAILSAFE_AUTH_USER
#define FAILSAFE_AUTH_USER ""
#endif

#ifndef FAILSAFE_AUTH_PASS
#define FAILSAFE_AUTH_PASS ""
#endif

#define FAILSAFE_BOOTSTATE_MAGIC 0x71AD //some random value to check if boot state is valid (to detect if rtc memory was cleared or brownout)
#define FAILSAFE_MAX_REBOOTS  5 //amount of reboots within the bootloop detection window to trigger failsafe mode
#define FAILSAFE_CLEAR_AFTER_SEC 90 //the time span on which reboots are counted for bootloop detection
#define FAILSAFE_RETRY_BOOT_SEC  5*60 //retry normal boot after this time in failsafe mode
#define FAILSAFE_HTTP_PORT 80
//TODO: calculate offset based on sizeof(BootState) and alignment requirements, and add static_assert to check at compile time
#define FAILSAFE_RTC_OFFSET 64 //offset in RTC memory to store boot state, must be 4-byte aligned and large enough to hold BootState struct (currently 8 bytes)

class FailsafeClass
{
    public:
        FailsafeClass();
        bool check();
        bool loop();

    private:
    //TODO: optimize to 4 bytes
    //TODO: maybe with crc? (2bytes magic, 1 byte boot_count, 1 byte crc)
        struct BootState { // 8 bytes total (2 word blocks) 
            uint32_t magic;
            uint32_t boot_count;
        };

        static_assert((sizeof(BootState) % 4) == 0, "BootState must be 4-byte aligned");

        void startFailsafeMode();

        void scheduleStableClear();

        void clearBootState();
        bool readBootState(BootState &state);
        void writeBootState(BootState &state);

        void setupWifiAp();
        void setupServer();
        
        bool checkAuth(AsyncWebServerRequest *request);
        const char *failsafePage();
        void handleRoot(AsyncWebServerRequest *request);
        void handleUpdate(AsyncWebServerRequest *request);
        void handleUpdateUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t* data, size_t len, bool final);
        void handleReset(AsyncWebServerRequest *request);
        void handleNotFound(AsyncWebServerRequest *request);
        void scheduleRestart(uint32_t delayMs);

        bool _active = false;
        bool _clearPending = false;
        uint32_t _clearAtMs = 0;
        uint32_t _failsafeStartMs = 0;
        bool _updateOk = true;
        bool _uploadSeen = false;
        bool _restartPending = false;
        uint32_t _restartAtMs = 0;
        bool _ledState = false;
        AsyncWebServer _server{FAILSAFE_HTTP_PORT};
};

extern FailsafeClass Failsafe;

/* vim:set ts=4 et: */
