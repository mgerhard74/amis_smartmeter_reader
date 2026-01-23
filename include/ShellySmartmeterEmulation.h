// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <map>
#include <Arduino.h>
#include <ESPAsyncUDP.h>

class ShellySmartmeterEmulationClass {
public:
    ShellySmartmeterEmulationClass();

    struct Device {
        String name;
        String prettyName;
        uint16_t port;
        String id;
    };

    //TODO: actually memory intensive. refactor as [] - and prettynames are only for log
    static inline const std::map<String, Device> DEVICES = {
        {"shellypro3em_old", {"shellypro3em", "Shelly 3 EM Pro (old)", 1010, ""}},
        {"shellypro3em",     {"shellypro3em", "Shelly 3 EM Pro", 2220, ""}},
        {"shellyemg3",       {"shellyemg3", "Shelly EM Gen3", 2222, ""}},
        {"shellyproem50",    {"shellyproem50", "Shelly Pro EM-50", 2223, ""}} 
    };

    void init(Device deviceConfig, int offset);
    bool setEnabled(bool enabled);
    bool enable();
    void disable();
    //NOTE: kept signature of function to match ModbusSmartmeterEmulation (actually 1.8.0 & 2.8.0 not needed)
    void setCurrentValues(bool dataAreValid, uint32_t v1_7_0=0, uint32_t v2_7_0=0, uint32_t v1_8_0=0, uint32_t v2_8_0=0);


private:
    AsyncUDP _udp;
    Device _device;
    String _deviceID;
    int _offset;    
    
    bool _enabled = false;

    struct {
        bool dataAreValid = false;
        int32_t saldo=0;
    } _currentValues;

    bool listenAndHandleUDP();

};


extern ShellySmartmeterEmulationClass ShellySmartmeterEmulation;

/* vim:set ts=4 et: */
