// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <map>
#include <Arduino.h>
#include <ESPAsyncUDP.h>

class ShellySmartmeterEmulationClass {
public:
    ShellySmartmeterEmulationClass();

    struct Device {
        uint16_t port;
        String id;
    };

    //NOTE: be careful, index of array must match the selected dropbox value of index.html
    static inline const Device DEVICES[4] = {
        {2223, "shellyproem50"},    //Shelly Pro EM-50
        {2222, "shellyemg3"},       //Shelly EM Gen3
        {2220, "shellypro3em"},     //Shelly Pro 3EM (Firmware >=224)
        {1010, "shellypro3em"}      //Shelly Pro 3EM (Firmware <224)
    };

    bool init(int selectedDeviceIndex, String customDeviceIDAppendix, int saldoOffset);
    bool setEnabled(bool enabled);
    bool enable();
    void disable();
    //NOTE: kept signature of function to match ModbusSmartmeterEmulation (actually 1.8.0 & 2.8.0 not needed)
    void setCurrentValues(bool dataAreValid, uint32_t v1_7_0=0, uint32_t v2_7_0=0, uint32_t v1_8_0=0, uint32_t v2_8_0=0);

private:
    AsyncUDP _udp;
    Device _device;
    int _offset;

    bool _enabled = false;

    struct {
        bool dataAreValid = false;
        int32_t saldo=0;
    } _currentValues;

    bool listen();
    void handleRequest(AsyncUDPPacket udpPacket);

};


extern ShellySmartmeterEmulationClass ShellySmartmeterEmulation;

/* vim:set ts=4 et: */
