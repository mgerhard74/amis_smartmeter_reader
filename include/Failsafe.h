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
#define FAILSAFE_BOOTSTATE_MAGIC_LSB 0xAD //some random value to check if boot state is valid (to detect if rtc memory was cleared or brownout)
#define FAILSAFE_BOOTSTATE_MAGIC_MSB 0x71 //some random value to check if boot state is valid (to detect if rtc memory was cleared or brownout)
#define FAILSAFE_MAX_REBOOTS  5 //amount of reboots within the bootloop detection window to trigger failsafe mode
#define FAILSAFE_CLEAR_AFTER_SEC 90 //the time span on which reboots are counted for bootloop detection
#define FAILSAFE_RETRY_BOOT_SEC  5*60 //retry normal boot after this time in failsafe mode
#define FAILSAFE_HTTP_PORT 80
//TODO: calculate offset based on sizeof(BootState) and alignment requirements, and add static_assert to check at compile time
#define FAILSAFE_RTC_OFFSET 64 //offset in RTC memory to store boot state, must be 4-byte aligned and large enough to hold BootState struct

class FailsafeClass
{
    public:
        FailsafeClass();
        bool check();
        bool loop();

    private:
        struct BootState {
            uint8_t _bootState[4]; //magic (2 bytes), boot count (1 byte), CRC8 (1 byte)

            BootState() {
                _bootState[0] = FAILSAFE_BOOTSTATE_MAGIC & 0xFF;
                _bootState[1] = (FAILSAFE_BOOTSTATE_MAGIC >> 8) & 0xFF;
                _bootState[2] = 0; //boot count
                _bootState[3] = 0; //CRC
            }

            void calcCRC8() {
                uint8_t crc = 0x00;
                for (std::size_t i = 0; i < 3; ++i) {
                    crc ^= _bootState[i];
                    for (int bit = 0; bit < 8; ++bit) {
                        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                                        : static_cast<uint8_t>(crc << 1);
                    }
                }
                _bootState[3] = crc;
            }   

            void setBootCount(uint8_t bootCount) {
                _bootState[2] = bootCount;
            }

            void setBootState(uint32_t bootState) {
                _bootState[0] = bootState & 0xFF;
                _bootState[1] = (bootState >> 8) & 0xFF;
                _bootState[2] = (bootState >> 16) & 0xFF;
                _bootState[3] = (bootState >> 24) & 0xFF;
            }

            uint32_t getBootState() {
                calcCRC8();
                return static_cast<uint32_t>(_bootState[0]) |
                    (static_cast<uint32_t>(_bootState[1]) << 8) |
                    (static_cast<uint32_t>(_bootState[2]) << 16) |
                    (static_cast<uint32_t>(_bootState[3]) << 24);
            }            

            uint16_t getMagic() {
                return static_cast<uint16_t>(_bootState[0]) |
                    static_cast<uint16_t>(_bootState[1] << 8);
            } 

            void setMagic(uint16_t magic) {
                _bootState[0] = magic & 0xFF;
                _bootState[1] = (magic >> 8) & 0xFF;
            }

            uint8_t getBootCount() {
                return _bootState[2];
            }

            uint8_t getCRC() {
                return _bootState[3];
            }

            void incrementBootCount() {
                setBootCount(getBootCount()+1);
            }

        } __attribute__((packed)) _bootState;

        static_assert((sizeof(BootState) % 4) == 0, "_bootState must be 4-byte aligned");

        void startFailsafeMode();

        void scheduleStableClear();

        void clearBootState();
        bool readBootState();
        void writeBootState();

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
