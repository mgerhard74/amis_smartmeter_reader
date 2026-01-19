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
        int port;
        String id;
    };

    // Die Map-Initialisierung
    static inline const std::map<String, Device> DEVICES = {
        {"shelly3empro_old", {"shelly3empro", "Shelly 3 EM Pro (old)", 1010, ""}},
        {"shelly3empro",     {"shelly3empro", "Shelly 3 EM Pro", 2220, ""}},
        {"shellyemg3",       {"shellyemg3", "Shelly EMG 3", 2222, ""}},
        {"shellyem50",       {"shellyem50", "Shelly Pro EM 50", 2223, ""}} 
    };

    void init(Device deviceConfig);
    bool setEnabled(bool enabled);
    bool enable();
    void disable();
    void setCurrentValues(bool dataAreValid, uint32_t v1_7_0=0, uint32_t v2_7_0=0, uint32_t v1_8_0=0, uint32_t v2_8_0=0);


private:
    AsyncUDP udp;
    Device _device;
    String _deviceID;
    
    
    bool _enabled = false;

    struct {
        bool dataAreValid = false;
        uint32_t v1_7_0=0, v2_7_0=0, v1_8_0=0, v2_8_0=0;
        uint32_t saldo=0;
    } _currentValues;

    bool listenAndHandleUDP();

};


extern ShellySmartmeterEmulationClass ShellySmartmeterEmulation;

/* vim:set ts=4 et: */
