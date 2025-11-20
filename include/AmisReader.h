#pragma once

#include <Arduino.h>
#include <time.h>

// TODO: Refactor this global vars
extern uint32_t a_result[10];
extern int valid;
extern char timecode[13];
extern uint8_t dow;
extern uint8_t mon, myyear;


typedef struct {
    bool isSet;
    struct tm time;
    char timecode[13]; // die "Aufbereitung" des timecodes
    uint32_t results[8];
    int32_t results_8;
} AmisReaderNumResult_t;


enum AmisReadSerialnumberMode_t {
    disabled,       // Serialnummer nicht lesen. Nur Zählerdaten lesen
    tryRead,        // Versuche Serialnummer zu lesen, wenns nicht funktioniert (Timeout) dann nur Zählerdaten lesen
    mustRead,       // Serialnummer muss gelesen werden, erst dann werden Zählerdaten gelesen
};


// Maximale Länge der Seriennummer
// siehe AMIS TD-351x Benutzerhandbuch // 5.2.5 und 5.2.8.1. Anforderungstelegramm (ev gesamt 5.2)
// ( https://wiki.volkszaehler.org/_media/hardware/channels/meters/power/siemens/amis_td-351x_bhbk.pdf )
// Wird auch als Geräteadresse, Sachnummer oder Identifikation bezeichnet
#define AMISREADER_MAX_SERIALNUMER 32

class AmisReaderClass {
public:
    void init(uint8_t serialNo);
    void loop();
    void setKey(const char *key);
    void setKey(const uint8_t *key);
    const char *getSerialNumber();
    void enable();
    void disable();
    // void enableRestart(); // enables and restart fresh readings

private:
    enum AmisReaderState_t {
        initReadSerial,         // serielle: baudrate usw einstellen
        requestReaderSerial,    // anfrage
        waitForReaderSerial,
        decodeReaderSerial,

        initReadCounters,
        // requestReaderCounter, // there is nothing to do as the counter sends the data each second
        readReaderCounters,
        decodeReaderCounter,

        //stateUndefined,
    };

    size_t serialWrite(const char *str);
    size_t serialWrite(uint8_t byte);
    size_t clearSerialRx();
    size_t pollSerial();

    //void eatSerialReadBuffer(size_t n); // ist derzeit nicht benötigt und mittels #if 0 dektiviert

    void processStateSerialnumber(const unsigned long msNow);
    void processStateCounters(const unsigned long msNow);

    void moveSerialBufferToDecodingWorkBuffer(size_t n);

    int decodeBuffer(uint8_t *buffer, size_t len, AmisReaderNumResult_t &result);

    HardwareSerial *_serial = nullptr;

    char _baudRateIdentifier;
    char _serialNumber[AMISREADER_MAX_SERIALNUMER + 1]; // auch als "Geräteadresse" oder "Identifikation" bezeichnet

    AmisReaderState_t _state = initReadSerial;
    //AmisReaderState_t _prevState = stateUndefined;
    unsigned long _stateLastSetMs;

    unsigned long _stateTimoutMs;
    unsigned int _stateErrorCnt;
    unsigned int _stateErrorMax;

    uint8_t _serialReadBuffer[128];
    size_t _serialReadBufferIdx=0;
    //unsigned long _lastSerialEvent;
    uint8_t _bytesInBufferExpectd;

    uint8_t _decodingWorkBuffer[128];
    size_t _decodingWorkBufferLength=0;

    uint8_t _key[16];

    AmisReadSerialnumberMode_t _readSerialNumberMode = tryRead;

    bool _isEnabled = false;
    bool _readerIsOnline = false;
};

extern AmisReaderClass AmisReader;

/* vim:set ts=4 et: */
