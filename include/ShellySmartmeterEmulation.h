// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <map>
#include <Arduino.h>
#include <ESPAsyncUDP.h>

class ShellySmartmeterEmulationClass {
public:
    ShellySmartmeterEmulationClass();

    bool init(unsigned selectedDeviceIndex, const char *customDeviceIDAppendix, int saldoOffset);
    bool setEnabled(bool enabled);
    bool enable();
    void disable();
    //NOTE: kept signature of function to match ModbusSmartmeterEmulation (actually 1.8.0 & 2.8.0 not needed)
    void setCurrentValues(bool dataAreValid, uint32_t v1_7_0=0, uint32_t v2_7_0=0, uint32_t v1_8_0=0, uint32_t v2_8_0=0);

private:
    typedef struct {
        uint16_t port;
        char id[27];
    } _device_t;
    _device_t _device;

    AsyncUDP _udp;
    bool _enabled = false;

    struct {
        bool dataAreValid = false;
        int32_t saldo=0;
    } _currentValues;
    int _offset; // Watt offset for saldo

    bool listen();
    void handleRequest(AsyncUDPPacket udpPacket);
};


extern ShellySmartmeterEmulationClass ShellySmartmeterEmulation;

/* vim:set ts=4 et: */
