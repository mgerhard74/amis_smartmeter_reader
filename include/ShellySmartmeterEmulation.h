// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <map>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncUDP.h>

// Capacity of a json document which is big enough to hold the "result" object
// of every RPC method supported by buildRpcResult() (biggest one: "EM.GetStatus").
#define SHELLY_RPC_RESULT_JSON_CAPACITY     (JSON_OBJECT_SIZE(25) + JSON_ARRAY_SIZE(0) + 288)

class ShellySmartmeterEmulationClass {
public:
    ShellySmartmeterEmulationClass();

    bool init(unsigned selectedDeviceIndex, const char *customDeviceIDAppendix, int saldoOffset);
    bool setEnabled(bool enabled);
    bool enable();
    //NOTE: disables the UDP *and* the HTTP API (used by RebootClass to stop the emulation completely)
    void disable();
    //NOTE: kept signature of function to match ModbusSmartmeterEmulation
    void setCurrentValues(bool dataAreValid, uint32_t v1_7_0=0, uint32_t v2_7_0=0, uint32_t v1_8_0=0, uint32_t v2_8_0=0);

    // "RPC over HTTP" (see Webserver_Shelly.cpp) and the mDNS announcement (see Network.cpp).
    // There is no socket to open/close - the requests are served by our regular webserver -
    // so enabling/disabling is just a flag.
    bool enableHttpApi();
    void disableHttpApi();
    bool httpApiEnabled() const { return _httpApiEnabled; }

    bool isInitialized() const { return _device.id[0] != '\0'; }
    bool hasValidData() const { return _currentValues.dataAreValid; }

    // Device properties needed by the HTTP API and by the mDNS announcement.
    // NOTE: The strings live in PROGMEM, so they are returned as __FlashStringHelper.
    //       ArduinoJson takes them as they are, everywhere else use String(...).
    const char *getDeviceId() const { return _device.id; }              // eg "shellypro3em-a8032abe1234"
    const char *getDeviceName() const { return _device.name; }          // eg "ShellyPro3EM-A8032ABE1234"
    const __FlashStringHelper *getApp() const;                          // eg "Pro3EM" (Shelly.GetDeviceInfo)
    const __FlashStringHelper *getMdnsApp() const;                      // eg "shellypro3em" (mDNS TXT record)
    const __FlashStringHelper *getModel() const;                        // eg "SPEM-003CEBEU"
    unsigned getGeneration() const;                                     // Shelly device generation
    unsigned getMdnsGeneration() const;                                 // generation announced via mDNS
    bool isTriphase() const;                                            // true: EM.* methods, false: EM1.* methods
    static const __FlashStringHelper *getFirmwareId();
    static const __FlashStringHelper *getFirmwareVersion();
    // Mac address of our device as a Shelly reports it: uppercase hex, no separators
    static String getMacAddress();

    // Builds the "result" object of a Shelly RPC method into 'result'.
    // With 'minimalResponse' only the values needed by the B2500 batteries are set
    // (see handleRequest() why we do not want to change the UDP responses).
    // Returns false if the method is not supported or if no valid meter data is available.
    bool buildRpcResult(const char *method, JsonObject result, bool minimalResponse = false);

private:
    typedef struct {
        uint16_t port;
        char name[14 + 16 + 1]; // longest prefix ("ShellyProEM50-") + longest appendix + '\0'
        char id[14 + 16 + 1];   // lowercase variant of 'name'
    } _device_t;
    _device_t _device;
    unsigned _deviceIndex = 0;

    AsyncUDP _udp;
    bool _enabled = false;
    bool _httpApiEnabled = false;

    struct {
        bool dataAreValid = false;
        int32_t saldo=0;
        uint32_t v1_8_0=0, v2_8_0=0;
    } _currentValues;
    int _offset; // Watt offset for saldo

    bool listen();
    void handleRequest(AsyncUDPPacket udpPacket);

    int32_t saldo() const { return _currentValues.saldo + _offset; }
    void buildEmGetStatus(JsonObject result, bool minimalResponse);
    void buildEm1GetStatus(JsonObject result, bool minimalResponse);
    void buildEmDataGetStatus(JsonObject result);
    void buildEm1DataGetStatus(JsonObject result);
    void buildEmGetConfig(JsonObject result);
    void buildDeviceInfo(JsonObject result);
    void buildListMethods(JsonObject result);
};


extern ShellySmartmeterEmulationClass ShellySmartmeterEmulation;

/* vim:set ts=4 et: */
