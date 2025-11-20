// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <ESPAsyncTCP.h>

#define SMARTMETER_EMULATION_SERVER_PORT 502

class ModbusSmartmeterEmulationClass {
public:
    ModbusSmartmeterEmulationClass();
    void init();
    bool setEnabled(bool enabled);
    bool enable();
    void disable();
    void setCurrentValues(bool dataAreValid, uint32_t v1_7_0=0, uint32_t v2_7_0=0, uint32_t v1_8_0=0, uint32_t v2_8_0=0);

private:
    AsyncServer _server;
    void clientOnClient(void* arg, AsyncClient* client);
    void clientOnError(void* arg, AsyncClient* client, int8_t error);
    void clientOnDisconnect(void* arg, AsyncClient* client);
    void clientOnTimeOut(void* arg, AsyncClient* client, uint32_t time);
    void clientOnData(void* arg, AsyncClient* client, void *data, size_t len);

    bool perpareBuffers();

    // Buffer for the Modbus registers from register number 40001 - 40197 (=Index 40000 - 40196)
    // So _modbus_reg_buffer must hold 197 register
    #define MODBUS_SMARTMETER_FIRST_AVAIL_REGNO (40001)
    #define MODBUS_SMARTMETER_LAST_AVAIL_REGNO  (40197)
    #define MODBUS_SMARTMETER_EMULATION_NUM_OF_REGS \
                ( MODBUS_SMARTMETER_LAST_AVAIL_REGNO - MODBUS_SMARTMETER_FIRST_AVAIL_REGNO + 1)
    #define MODBUS_SMARTMETER_FIRST_AVAIL_REGIDX (MODBUS_SMARTMETER_FIRST_AVAIL_REGNO - 1)
    #define MODBUS_SMARTMETER_LAST_AVAIL_REGIDX (MODBUS_SMARTMETER_LAST_AVAIL_REGNO - 1)

    uint16_t *_modbus_reg_buffer;

    bool _enabled = false;
    unsigned int _clientsConnectedCnt; // Number of current connected clients

    // Die MODBUS Geräte ID, auf welche reagiert werden soll
    // Bei Werten größer 255 wird immer geantwortet
    const unsigned int _unitId = 256;

    struct {
        bool dataAreValid;
        uint32_t v1_7_0, v2_7_0, v1_8_0, v2_8_0;
    } _currentValues;
    struct {
        uint32_t v1_7_0, v2_7_0, v1_8_0, v2_8_0;
    } _valuesInBuffer;
};


extern ModbusSmartmeterEmulationClass ModbusSmartmeterEmulation;

/* vim:set ts=4 et: */
