#include "AmisReader.h"

#include "aes.h"
#include "Databroker.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_AMISREADER
#include "ModbusSmartmeterEmulation.h"
#include "RebootAtMidnight.h"
#include "RemoteOnOff.h"
#include "ShellySmartmeterEmulation.h"
#include "SystemMonitor.h"
#include "ThingSpeak.h"
#include "UA.h"
#include "Utils.h"


// TODO(anyone): Refactor this global vars and external function
extern bool new_data_for_websocket;
extern unsigned first_frame;
uint8_t dow;
uint8_t mon, myyear;


// ---------------------------------------------------------------------------------------------------------------------------

struct __attribute__((__packed__))  enryptedMBUSTelegram_SND_UD {
    // 2021-11-05-spezifikation-ks-amis.pdf Page 6 - 2.4.2.9. 'Beispiel: Zählerdaten [hex]'
    // Unser Lesekopf ist eigentlich ein "M-BUS-Slave" und wir erhalten periodisch
    // eine 'Long Frame' Antwort
    // siehe https://m-bus.com/documentation-wired/05-data-link-layer

    // Offset 0
    struct __attribute__((__packed__)) {
        struct __attribute__((__packed__)) {  // 0x68 0x5F 0x5F 0x68 0x53
            uint8_t startSign;           // 0x68
            uint8_t telegramLength;      // 0x5F
            uint8_t telegramLengthRepeat;// 0x5F
            uint8_t startSignRepeat;     // 0x68
            uint8_t cField;              // 0x53 oder 0x73
        } SND_UD;
    // Offset 5
        uint8_t primaryAddress;          // 0xF0
        uint8_t ci;                      // 0x5B = 'variable data respond'
    // Offset 7
        struct __attribute__((__packed__)) {
            uint8_t identificationNumber[4]; // TODO(anyone): Zählernummerr?? 0x00, 0x00, 0x00, 0x00
            uint16_t vendorId;               // Hersteller Identifikation EMH, LSB first 0x2d 0x4c
            uint8_t version;                 // 0x01
            uint8_t medium;                  // 02 = Medium Elektrizität --> hier 0x0e
        } secondaryAddress;
    // Offset 15
        uint8_t accessNumber;            // accesscounter (laufender Zähler - erhöht sich bei jeder Ausgabe)
        uint8_t status;                  // undefined              0x50
        uint16_t signature;              // undefined              0x05
    } header;
    // Offset 19
    uint8_t encryptedData[80];
    // Offset 99
    uint8_t checkSum;                // cheksum(ByteAdd) from(including) primaryAddress to(including) encryptedData[79]
    uint8_t stopSign;                // 0x16
};
static_assert(sizeof(struct enryptedMBUSTelegram_SND_UD) == 101);

struct __attribute__((__packed__))  decryptedTelegramData_SND_UD {
    uint8_t filler_prev[2];     // 0x2f 0x2f

    uint8_t difVifDT[2];        // 0x06 / 0x6d
    uint8_t valueDT[6];         // Date+Time [M-Bus CP48]

    uint8_t difVif1_8_0[2];     // 0x04 / 0x03
    uint32_t value1_8_0;        // Zählerstand Energie A+ [Wh]

    uint8_t difVif2_8_0[3];     // 0x04 / 0x83 0x3C
    uint32_t value2_8_0;        // Zählerstand Energie A- [Wh]

    uint8_t difVif3_8_1[5];     // 0x84 / 0x10 0xFB 0x82 0x73
    uint32_t value3_8_1;        // Zählerstand Energie R+ [Wh]

    uint8_t difVif4_8_1[6];     // 0x84 / 0x10 0xFB 0x82 0xF3 0x3C
    uint32_t value4_8_1;        // Zählerstand Energie R- [Wh]

    uint8_t difVif1_7_0[2];     // 0x04 / 0x2B
    uint32_t value1_7_0;        // momentane Wirkleistung P+

    uint8_t difVif2_7_0[3];     // 0x04 / 0xAB 0x3C
    uint32_t value2_7_0;        // momentane Wirkleistung P-

    uint8_t difVif3_7_0[3];     // 0x04 / 0xFB 0x14
    uint32_t value3_7_0;        // momentane Blindleistung Q+

    uint8_t difVif4_7_0[4];     // 0x04 / 0xFB 0x94 0x3C
    uint32_t value4_7_0;        // momentane Blindleistung Q-

    uint8_t difVif1_128_0[4];   // 0x04 / 0x83 0xFF 0x04
    int32_t value1_128_0;       // Inkassozählwerk

    uint8_t filler_tail[2];     // 0x2f 0x2f
};
static_assert(sizeof(struct decryptedTelegramData_SND_UD) == 80);

// ---------------------------------------------------------------------------------------------------------------------------

static void setTime(time_t ts_now) {
    time_t tv_sec_old;
    tv_sec_old = time(NULL);
    if (ts_now == tv_sec_old) {
        LOG_DP("Time already in sync");
        return; // skip if we're already in sync!
    }
    // Transfer of enryptedMBUSTelegram_SND_UD (101 bytes) needs ~105ms at 9600 8N1
    struct timeval ti;
    ti.tv_sec = ts_now;
    ti.tv_usec = 105000;
    settimeofday(&ti, NULL);

    LOGF_IP("Time synchronized. (ts-old=%llu, ts-now=%llu, millis=%u)", tv_sec_old, ts_now, millis());
    RebootAtMidnight.adjustTicker();
}

int AmisReaderClass::decodeBuffer(uint8_t *buffer, size_t len, AmisReaderNumResult_t &result)
{
    // This decoding functions needs about 1-2 ms!

    result.isSet = false;

    AmisReaderNumResult_t numresult;
    const enryptedMBUSTelegram_SND_UD *encryptedSndUD = (const enryptedMBUSTelegram_SND_UD *) buffer;

    // Here we expect got a MBUS - SND_UD (Send User Data to Slave)
    // Refer to: https://m-bus.com/documentation-wired/05-data-link-layer
    //                     5.2 Telegram Format - Long Frame
    //                     5.3 Meaning of the Fields

    // check the minimal needed length
    if (len < sizeof(enryptedMBUSTelegram_SND_UD)) {
        return -1;
    }

    // check fields 'header' and 'repeat header' have expected value
    if(encryptedSndUD->header.SND_UD.startSign!=0x68 || encryptedSndUD->header.SND_UD.startSignRepeat!=0x68) {
        return -2;
    }

    // check fields 'length' and 'repeat length' have same value
    if(encryptedSndUD->header.SND_UD.telegramLength != encryptedSndUD->header.SND_UD.telegramLengthRepeat) {
        return -3;
    }

    // check the length as we expected
    if (encryptedSndUD->header.SND_UD.telegramLength != 0x5f) {
        return -4;
    }

    // check 'C Field (Control Field, Function Field)' have expected value
    if (encryptedSndUD->header.SND_UD.cField != 0x53 && encryptedSndUD->header.SND_UD.cField != 0x73) {
        return -5;
    }

    // check the expected "Stop" value in SND_UD
    if (encryptedSndUD->stopSign != 0x16) {
        return -6;
    }

    // create and check checksum
    uint8_t checksum=0;
    const uint8_t *chk_s = (const uint8_t *) &encryptedSndUD->header.SND_UD.cField;
    const uint8_t *chk_e = (const uint8_t *) &encryptedSndUD->checkSum;
    while (chk_s < chk_e) {
        checksum += *chk_s++;
    }
    if (checksum != encryptedSndUD->checkSum) {
        return -7;
    }

    // Decrypt
    uint8_t initialVector[16];
    const uint8_t *secondaryAddress = (const uint8_t *) &encryptedSndUD->header.secondaryAddress;
    initialVector[0] = secondaryAddress[4]; // =encryptedSndUD->secondaryAddress.vendorId/1
    initialVector[1] = secondaryAddress[5]; // =encryptedSndUD->secondaryAddress.vendorId/2
    initialVector[2] = secondaryAddress[0]; // =encryptedSndUD->secondaryAddress.identificationNumber[0]
    initialVector[3] = secondaryAddress[1]; // =encryptedSndUD->secondaryAddress.identificationNumber[1]
    initialVector[4] = secondaryAddress[2]; // =encryptedSndUD->secondaryAddress.identificationNumber[2]
    initialVector[5] = secondaryAddress[3]; // =encryptedSndUD->secondaryAddress.identificationNumber[3]
    initialVector[6] = secondaryAddress[6]; // =encryptedSndUD->secondaryAddress.version
    initialVector[7] = secondaryAddress[7]; // =encryptedSndUD->secondaryAddress.medium
    for (size_t i=8; i<16; ++i) {
        initialVector[i] = secondaryAddress[8]; // =encryptedSndUD->secondaryAddress.accessNumber
    }

    struct decryptedTelegramData_SND_UD decrypted; // wir decodieren 80 Bytes ( 5 * 16 )
    uint8_t *decrypted_ptr = (uint8_t *) &decrypted;
#if 0
    AES128_CBC_decrypt_buffer(decrypted_ptr,    &encryptedSndUD->encryptedData[0],  16, _key, initialVector);
    AES128_CBC_decrypt_buffer(decrypted_ptr+16, &encryptedSndUD->encryptedData[16], 16, nullptr, nullptr);
    AES128_CBC_decrypt_buffer(decrypted_ptr+32, &encryptedSndUD->encryptedData[32], 16, nullptr, nullptr);
    AES128_CBC_decrypt_buffer(decrypted_ptr+48, &encryptedSndUD->encryptedData[48], 16, nullptr, nullptr);
    AES128_CBC_decrypt_buffer(decrypted_ptr+64, &encryptedSndUD->encryptedData[64], 16, nullptr, nullptr);
#else
    AES128_CBC_decrypt_buffer(decrypted_ptr,    &encryptedSndUD->encryptedData[0],  16*5, nullptr, initialVector);
#endif

    if (decrypted.filler_prev[0] != 0x2f || decrypted.filler_prev[1] != 0x2f) {
        return -8; // Füllbytes vorne
    }
    if (decrypted.filler_tail[0] != 0x2f || decrypted.filler_tail[1] != 0x2f) {
        return -9; // Füllbytes hinten
    }

    // DIF/VIFs prüfen
    if (memcmp(&decrypted.difVifDT[0], "\x06\x6d", 2) != 0) {
        // M-Bus DIF/VIFs - für Datum/Uhrzeit
        return -10;
    }
    if (memcmp(&decrypted.difVif1_8_0[0], "\x04\x03", 2) != 0) {
        // M-Bus DIF/VIFs - für Zählerstand Energie A+ [Wh]
        return -11;
    }
    if (memcmp(&decrypted.difVif2_8_0[0], "\x04\x83\x3c", 3) != 0) {
        // M-Bus DIF/VIFs - für Zählerstand Energie A- [Wh]
        return -12;
    }
    if (memcmp(&decrypted.difVif3_8_1[0], "\x84\x10\xFB\x82\x73", 5) != 0) {
        // M-Bus DIF/VIFs - für Zählerstand Energie R+ [Wh]
        return -13;
    }
    if (memcmp(&decrypted.difVif4_8_1[0], "\x84\x10\xFB\x82\xf3\x3c", 6) != 0) {
        // M-Bus DIF/VIFs - für Zählerstand Energie R+ [Wh]
        return -14;
    }
    if (memcmp(&decrypted.difVif1_7_0[0], "\x04\x2b", 2) != 0) {
        // M-Bus DIF/VIFs - für momentane Wirkleistung P+
        return -15;
    }
    if (memcmp(&decrypted.difVif2_7_0[0], "\x04\xab\x3c", 3) != 0) {
        // M-Bus DIF/VIFs - für momentane Wirkleistung P-
        return -16;
    }
    if (memcmp(&decrypted.difVif3_7_0[0], "\x04\xfb\x14", 3) != 0) {
        // M-Bus DIF/VIFs - für momentane Blindleistung Q+
        return -17;
    }
    if (memcmp(&decrypted.difVif4_7_0[0], "\x04\xfb\x94\x3c", 4) != 0) {
        // M-Bus DIF/VIFs - für momentane Blindleistung Q-
        return -18;
    }
    if (memcmp(&decrypted.difVif1_128_0[0], "\x04\x83\xff\x04", 4) != 0) {
        // M-Bus DIF/VIFs - für Inkassozählwerk
        return -19;
    }

    // Datum & Uhrzeit rausholen und validieren
    if (!Utils::MbusCP48IToTm(numresult.time, decrypted.valueDT)) {
        return -20;
    }


    // timeCp48Hex: 10 stelliges hexformat (= 5 bytes) welches 1:1 vom Zähler übernommen wird
    // Ist eigentlich das MBUS CP48 Format - hier jedoch nur 5 Bytes
    //                 40    32 31    24 23    16 15     8 7      0
    //                 YYYYMMMM YYYDDDDD WWWhhhhh 0mmmmmmm Xsssssss    Webclient Verwendung
    //          beim Jahr(Y) muss noch 2000 hinzugefügt werden
    //          0 muss 0 sein (sonst ungültig)
    //          X könnte unter Umständen Info ob Schaltjahr??
    //          WWW ist der Wochentag (1=Mon, 2=Die, .... 7=Son)
    //          s:7 Bits, m:7 Bits, h:5 Bits, W:3 Bits, D:5 Bits, M:4 Bits, Y:7 Bits
    //
    // Das ganze gleich als Hexstring, weil wir nur 4 Byte Variablen mit json ausgeben können
    // (siehe auch: #define ARDUINOJSON_USE_LONG_LONG )
    snprintf(numresult.timeCp48Hex, sizeof(numresult.timeCp48Hex),
             "%02x%02x%02x%02x%02x",
             decrypted.valueDT[4], decrypted.valueDT[3], decrypted.valueDT[2],
             decrypted.valueDT[1], decrypted.valueDT[0]
    );

    // Zahlenwerte noch kopieren
    numresult.results_u32[0] = UA::ReadU32LE(&decrypted.value1_8_0);
    numresult.results_u32[1] = UA::ReadU32LE(&decrypted.value2_8_0);
    numresult.results_u32[2] = UA::ReadU32LE(&decrypted.value3_8_1);
    numresult.results_u32[3] = UA::ReadU32LE(&decrypted.value4_8_1);
    numresult.results_u32[4] = UA::ReadU32LE(&decrypted.value1_7_0);
    numresult.results_u32[5] = UA::ReadU32LE(&decrypted.value2_7_0);
    numresult.results_u32[6] = UA::ReadU32LE(&decrypted.value3_7_0);
    numresult.results_u32[7] = UA::ReadU32LE(&decrypted.value4_7_0);
    numresult.results_i32[0] = UA::ReadS32LE(&decrypted.value1_128_0);

    numresult.isSet = true;
    result = numresult; // memcpy(&result, &numresult, sizeof(numresult));

    return 0;
}

const char *AmisReaderClass::getSerialNumber()
{
    if (_serialNumber[0]) {
        return _serialNumber;
    }
    return "unbekannt";
}

size_t AmisReaderClass::serialWrite(const char *str)
{
    size_t r;
    if (_serial) {
        r = _serial->write(str);
        _serial->flush();
    } else {
        r = strlen(str);
    }
    /*if (false) {
        MsgOut.writeTimestamp();
        MsgOut.printf("serialWrite()[%u]:\r\n", strlen(str));
        MsgOut.dumpHexBytes(str);
    }*/
    return r;
}

size_t AmisReaderClass::serialWrite(uint8_t byte)
{
    size_t r;
    if (_serial) {
        r = _serial->write(byte);
    } else {
        r = sizeof(byte);
    }
    /*if (false) {
        MsgOut.writeTimestamp();
        MsgOut.println("serialWrite()[1]:");
        MsgOut.dumpHexBytes(&byte, sizeof(uint8_t));
    }*/
    return r;
}

#if (AMISREADER_SIMULATE_SERIAL)
size_t AmisReaderClass::pollSerialSimulateSerialInput()
{
    static uint32_t lastSent = 0;

    uint32_t now = millis();
    if (now - lastSent < 1000) {
        return 0;
    }

    if(!_serialReadBufferIdx) {
        if (_state == waitForReaderSerial) {
            strlcpy((char*)_serialReadBuffer, "/SAT63511D-SiMuLaToR-\r\n", sizeof(_serialReadBuffer));
            _serialReadBufferIdx = strlen((const char*)_serialReadBuffer);
            lastSent = now;
        } else if (_state == readReaderCounters) {
            uint8_t demodata[106] = { // Demodaten für den Key '1234567890abcdef1234567890abcdef' (2025/12/18 18:34:41)
                0x10, 0x40, 0xF0, 0x30, 0x16,
                0x68, 0x5f, 0x5f, 0x68, 0x53, 0xf0, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x4c, 0x01, 0x0e, 0x00,
                0x50, 0x05, 0x00, 0xf3, 0xd2, 0xd5, 0xaf, 0x0d, 0xf9, 0xe4, 0x3e, 0xcd, 0x57, 0xc5, 0x1e, 0xf1,
                0x57, 0x93, 0x62, 0x5c, 0x98, 0x7f, 0x71, 0x63, 0xc9, 0x5a, 0x4a, 0xec, 0xf4, 0xfc, 0x1d, 0xf1,
                0x93, 0x68, 0x27, 0xd8, 0xf0, 0x01, 0x42, 0x84, 0x15, 0xa2, 0x98, 0xda, 0x6d, 0xe5, 0x27, 0x9f,
                0xa8, 0x2b, 0x7c, 0x84, 0xa6, 0x05, 0xd2, 0x5d, 0x92, 0x6e, 0xf3, 0x93, 0x29, 0x9a, 0x67, 0x4b,
                0x16, 0xd2, 0x6f, 0x68, 0x8d, 0x0b, 0x33, 0xbe, 0x5f, 0xc4, 0xc5, 0x1c, 0x79, 0x2a, 0x6a, 0x7b,
                0x5a, 0x29, 0x1e, 0xdd, 0x16
            };
            memcpy(_serialReadBuffer, demodata, sizeof(demodata));
            _serialReadBufferIdx = sizeof(demodata);
            lastSent = now;
        }
    }

    return _serialReadBufferIdx;
}
#endif // (AMISREADER_SIMULATE_SERIAL)


size_t AmisReaderClass::pollSerial()
{
#if (AMISREADER_SIMULATE_SERIAL)
    return pollSerialSimulateSerialInput();
#else
    if (_serial == nullptr) {
        return 0;
    }

    size_t bytesReadTotal = 0;
    size_t serialBufferLeft = sizeof(_serialReadBuffer) - _serialReadBufferIdx;
    size_t serialAvail = _serial->available();
    size_t bytesRead;
    size_t serialReadBufferIdxStarted = _serialReadBufferIdx;
    bool rxError = false & _serial->hasRxError();

    while (serialAvail > 0 && serialBufferLeft > 0 && !rxError) {
        bytesRead = _serial->readBytes(&_serialReadBuffer[_serialReadBufferIdx], std::min(serialAvail, serialBufferLeft));
        bytesReadTotal += bytesRead;
        serialBufferLeft -= bytesRead;
        _serialReadBufferIdx += bytesRead;
        serialAvail = _serial->available();
        rxError = false & _serial->hasRxError(); // INFO: This call clears the pending error flag
    }

    if ((serialBufferLeft == 0 && serialAvail > 0) || rxError ) {
        // No space in buffer left and still something available or a rx error occured
        // Eat all from the searial port and discard everything as we had an overflow
        // ERROR: Serial input overflowed!

        /*
        MsgOut.writeTimestamp();
        if (bytesBufferLeft == 0 && avail > 0) {
            MsgOut.printf("ERROR: Serial interface OVERRUN: SerAvailable:%u\r\n", avail);
        } else {
            MsgOut.printf("ERROR: Serial interface RX-Error SerAvailable:%u BufferSpaceLeft:%u\r\n", avail, bytesBufferLeft);
        }
        */

        _serialReadBufferIdx = 0;
        bytesRead = clearSerialRx();
        bytesReadTotal += bytesRead;
    }
    if (bytesReadTotal) {
        if (_serialReadBufferIdx > serialReadBufferIdxStarted) {
          /*
            if (false) {
                MsgOut.writeTimestamp();
                MsgOut.printf("Startlength: %u\r\n", serialReadBufferIdxStarted);
                MsgOut.printf("pollSerial() Total length now in buffer: %u\r\n", _serialReadBufferIdx);
                MsgOut.dumpHexBytes(&_serialReadBuffer[serialReadBufferIdxStarted], _serialReadBufferIdx - serialReadBufferIdxStarted, true);
            }
            */
        }
    }
    return bytesReadTotal;
#endif // AMISREADER_SIMULATE_SERIAL
}

size_t AmisReaderClass::clearSerialRx()
{
    /* Alles aus dem internen Puffer der Seriellen Schnittstelle auslesen und die Anzahl der Bytes zurückgeben */
    size_t bytesRead = 0;
    if (_serial == nullptr) {
        return 0;
    }

    size_t avail = _serial->available();
    while (avail > 0) {
        char b[128];
        bytesRead += _serial->readBytes(&b[0], std::min(avail, (size_t)sizeof(b)));
        avail = _serial->available();
    }
     _serial->hasRxError(); // This clears the pending error flag
    return bytesRead;
}


void AmisReaderClass::init(uint8_t serialNo)
{
    _baudRateIdentifier = 0; _serialNumber[0] = 0;
    _serialReadBufferIdx = 0;

    memset(&Databroker, 0, sizeof(Databroker));

    if (serialNo == 1) {
        _serial = &Serial;
    } else if (serialNo == 2) {
        _serial = &Serial1;
    } else {
        _serial = nullptr;
    }
}

void AmisReaderClass::end()
{
    disable();
    if (_serial != nullptr) {
        _serial->end();
        _serial = nullptr;
    }

    Databroker.valid = 0;
    new_data_for_websocket = false;

    _readerIsOnline = false;

    ModbusSmartmeterEmulation.setCurrentValues(false);
    ThingSpeak.onNewData(false);
}

void AmisReaderClass::processStateSerialnumber(const uint32_t msNow)
{
    if (_state == initReadSerial) {
        LOG_DP("Initalizing reading serialnumber");
        _baudRateIdentifier = 0; _serialNumber[0] = 0;

        if (_readSerialNumberMode == disabled ) {
            _state = initReadCounters;
            return;
        }
        if (_serial != nullptr) {
            _serial->begin(300, SERIAL_7E1); /* Serialnummer wird mit 300 7E1 gelesen */
            _serial->setTimeout(5);         // eigentlich prüfen wir ja mit available() ... aber vorsichtshalber
            _serial->clearWriteError();
        }
        _serialReadBufferIdx = 0;
        clearSerialRx();
        _readerIsOnline = false;
        _state = requestReaderSerial;
        _stateLastSetMs = msNow;
        _stateTimoutMs = 5000;
        _stateErrorCnt = 0;
        _stateErrorMax = 13; // 5 sec * 13 ... we try for 65 seconds
        _validFrameCnt = 0;
    } else if (_state == requestReaderSerial) {
        // Geräteadresse lesen (IEC 62056-21 Mode "C")
        // Senden des Anforderungstelegramm
        // /?!\r\n (hex 2F 3F 21 0D 0A)
        // siehe AMIS TD-351x Benutzerhandbuch
        _serialReadBufferIdx = 0;
        clearSerialRx();
        LOG_DP("Requesting serialnumber");
        serialWrite("/?!\r\n");
        // vor dem nächsten Senden muss zwischen 1500 und 2200ms gewartet werden
        // Übertragen der Gerätenummer dauert etwa 1200ms
        // Also: Wir warten 5 Sekunden - wenn dann keine Seriennummer --> nochmal lesen
        _state = waitForReaderSerial;
    } else if (_state == waitForReaderSerial) {
        // wir erwarten uns einen Antwort die wie folgt aussieht:
        // "/SATx1234567890123\r\n", wobei "x" 0,1,2,3,4,5,6 oder 9 sein darf
        //     x ... gibt die Baudrate der Datentelegramme im Mode C
        //     siehe AMIS TD-351x Benutzerhandbuch - Kapitel 5.2.5 ff  bzw.  gesamtes 5.2 Kapitel
        if (_serialReadBufferIdx < 4) {
            return;
        }
        if (memcmp(_serialReadBuffer, "/SAT", 4)) {
            LOGF_VP("Got %u bytes: %02x %02x %02x %02x ....",
                        _serialReadBufferIdx,
                        _serialReadBuffer[0], _serialReadBuffer[1],
                        _serialReadBuffer[2], _serialReadBuffer[3]);
            _state = requestReaderSerial;
            return;
        }
        if (_serialReadBufferIdx <= 7) {
            // wir brauchen auf jeden Fall 8 Byte:  "/SATxY\r\n"
            return;
        }

        if (_serialReadBufferIdx > 4 + AMISREADER_MAX_SERIALNUMER + 2) {
            // Zuviel Input (das wäre eine ganze Serialnummer gewesen) ... alles wegwerfen und nochmals probieren
            _state = requestReaderSerial;
            LOG_WP("Serialnumber invalid response #2 (Too much data)");
            return;
        }

        char *crlf;
        crlf = (char *) memmem(_serialReadBuffer+6, _serialReadBufferIdx-6, "\r\n", 2);
        if (!crlf) {
            return;
        }
        *crlf = 0;

        // OK wir haben irgendwas, das grundsätzlich schon mal gültig aussieht
        if ((_serialReadBuffer[4] >= '0' && _serialReadBuffer[4] <= '6') || _serialReadBuffer[4] == '9') {
            // Wir haben eine "gültige" Zähler-Antwort bekommen!

            // "0" ...  300 Bd, "1" ...  600 Bd, "2" ...  1200 Bd, "3" ...   2400 Bd
            // "4" ... 4800 Bd, "5" ... 9600 Bd, "6" ... 19200 Bd. "9" ... 115200 Bd
            _baudRateIdentifier = _serialReadBuffer[5];

            const char *serialNr = (const char *)_serialReadBuffer+5;
            strlcpy(_serialNumber, serialNr, sizeof(_serialNumber));

            _state = initReadCounters;
            _stateLastSetMs = msNow;
            _stateErrorCnt = 0;
            LOGF_IP("Serialnumber %s found", _serialNumber);

            //serialWrite("\x06" "060\r\n");
        } else {
            // das scheint ungültig zu sein ... alles wegwerfen und nochmals probieren
            _state = requestReaderSerial;
            LOG_WP("Serialnumber invalid response #1 (Invalid baudrate identifier)");
        }
        return;
    }
}

#if 0
void AmisReaderClass::eatSerialReadBuffer(size_t n)
{
    size_t i;
    if (n >= _serialReadBufferIdx || n >= sizeof(_serialReadBuffer)) {
        _serialReadBufferIdx = 0;
        return;
    }

    for(i=n; i<_serialReadBufferIdx; i++) {
        _serialReadBuffer[i-n] = _serialReadBuffer[i];
    }
    _serialReadBufferIdx -= n;
}
#endif

void AmisReaderClass::moveSerialBufferToDecodingWorkBuffer(size_t n)
{
// Die Daten aus dem Puffer der Seriallen Schnittstelle in den "Arbeitspuffer"
// zum Zählerstand dekodieren verschieben
    size_t i;

    if (n > std::min(sizeof(_decodingWorkBuffer),sizeof(_serialReadBuffer))) {
        n = std::min(sizeof(_decodingWorkBuffer),sizeof(_serialReadBuffer));
    }
    if (n > _serialReadBufferIdx) {
        n = _serialReadBufferIdx;
    }
    if (n == 0) {
        _decodingWorkBufferLength = 0;
        return;
    }
    memcpy(_decodingWorkBuffer, _serialReadBuffer, n);
    _decodingWorkBufferLength = n;
    for(i = 0; n < _serialReadBufferIdx;) {
        _serialReadBuffer[i++] = _serialReadBuffer[n++];
    }
    _serialReadBufferIdx = i;
}

void AmisReaderClass::processStateCounters(const uint32_t msNow)
{
    if (_state == initReadCounters) {
        LOG_DP("Initalizing reading counter values");
        if (_serial) {
            _serial->begin(9600, SERIAL_8E1); /* Zählerdaten kommen angeblich mit 9600 8E1 */
            _serial->setTimeout(5);           // eigentlich prüfen wir ja mit available() ... aber vorsichtshalber
            _serial->clearWriteError();
        }
        _serialReadBufferIdx = 0;
        clearSerialRx();
        _bytesInBufferExpectd = 0;
        _readerIsOnline = false;
        _state = readReaderCounters;
        _stateLastSetMs = msNow;
        _stateTimoutMs = 4000; // Timeout für das Lesen: 16 * 4 (=64 Sekunden)
        _stateErrorCnt = 0;
        _stateErrorMax = 16;
        _validFrameCnt = 0;
    } else if (_state == readReaderCounters) {
        if (_bytesInBufferExpectd == 0) {
            // we're waiting of starting message
            /*
            2.2.2. Suchmodus
                Der AMIS-Zähler führt im 1-min-Takt eine Suchabfrage (SND_NKE auf die
                Primäradresse „240“) nach einem geeigneten Endgerät durch. Dieser muss das
                Telegramm mit einem Acknowledgement („E5h“) quittieren.
            */
            if (_serialReadBufferIdx >= 5) {
                if (!memcmp(_serialReadBuffer, "\x10\x40\xF0\x30\x16", 5)) {
                    // was it message "<DLE>@ð0<SYN> ??  (= SND_NKE)
                    //MsgOut.writeTimestamp();
                    //MsgOut.println("Received SND_NKE");
                    LOG_DP("Received SND_NKE, sending ACK");
                    serialWrite(0xe5); // send ACK
                    for(size_t i=5; i<_serialReadBufferIdx; i++) {
                        _serialReadBuffer[i-5] = _serialReadBuffer[i];
                    }
                    _serialReadBufferIdx -= 5;
                } else if (_serialReadBuffer[0]==0x68 && _serialReadBuffer[3]==0x68) {
                    // seems we're already within SND_UD the message - so grab the expected length

                    /* Sample (101 bytes):
                            68 5F 5F 68 73 F0 5B 00  00 00 00 2D 4C 01 0E 00
                            00 50 05 1E 68 B1 D1 6E  87 0D AA 09 72 96 2D 60
                            2C F4 4F 6A 9E C6 FE 78  09 71 CD 49 D9 1B 9E 36
                            9F 7B 45 AF FB 57 E4 CF  98 DF 04 26 F9 B0 10 B0
                            BF B0 BE 01 CF 4C 6F F4  DC 65 BF 3B 6A 53 7A C7
                            CF 35 B5 00 53 E4 7E 33  4F 23 63 31 7F 42 BD 80
                            36 1A 52 41 16
                    */
                    // Expected Length should be 0x5F (see structure enryptedMBUSTelegram_SND_UD)
                    _bytesInBufferExpectd = _serialReadBuffer[1]+6;
                    LOGF_DP("Found SND_UD head. Expected length: %u", _bytesInBufferExpectd);

                    if (_bytesInBufferExpectd != 0x5f + 6) {
                        _serialReadBufferIdx = 0;
                        clearSerialRx();
                        _bytesInBufferExpectd = 0;
                    }
                }
            }
            return;
        }

        if (_serialReadBufferIdx >= _bytesInBufferExpectd) {
            serialWrite(0xe5); // received data must be confirmed

            LOGF_DP("Got %u bytes (expected %u)", _serialReadBufferIdx, _bytesInBufferExpectd);

            moveSerialBufferToDecodingWorkBuffer(_bytesInBufferExpectd);
            _bytesInBufferExpectd = 0;

            _serialReadBufferIdx = 0;
            clearSerialRx();

            _state = decodeReaderCounter;
        }
    } else if (_state == decodeReaderCounter) {
        // wir haben genug Daten empfangen und versuchen diese nun zu decodieren
        // Da das einiges Zeit in Anspruch nimmt, machen wir da hier in einem eigene State
        // Es war ja sogar im ursprünglichen amis_decoder() ein Aufruf von yield() enthalten
        // Anmerkung: Bei mir braucht die Dekodierung etwas 1-2ms

        Databroker.valid = 0;

        AmisReaderNumResult_t l_result;
        int r;
        r = decodeBuffer(_decodingWorkBuffer, _decodingWorkBufferLength,  l_result);
        if (r == 0) {
            // OK, we got new valid data ...
            _validFrameCnt++;

            // Move them to Databroker
            static_assert(std::size(Databroker.results_u32) == std::size(l_result.results_u32));
            for (size_t i=0;i < std::size(Databroker.results_u32); i++) {
                Databroker.results_u32[i] = l_result.results_u32[i];
            }
            static_assert(std::size(Databroker.results_i32) == std::size(l_result.results_i32));
            for (size_t i=0;i < std::size(Databroker.results_i32); i++) {
                Databroker.results_i32[i] = l_result.results_i32[i];
            }

            strlcpy(Databroker.timeCp48Hex, l_result.timeCp48Hex, sizeof(Databroker.timeCp48Hex));  // Hextsring der MBUS-CP48 Bytes[aber NUR 5 Bytes] (wie vom Zähler bekommen)
            memcpy(&Databroker.time, &l_result.time, sizeof(Databroker.time));  // struct tm (gefüllt - wie vom Zähler bekommen)
            Databroker.ts = mktime(&l_result.time); // Seconds since epoch in UTC (umgewandelt von l_result.time)

            Databroker.valid = 5;

            // Now all new data should be fetched only from Databroker!

            new_data_for_websocket = true;
            if (first_frame == 0) {
                first_frame = 3; // Erster Datensatz nach Reboot oder verlorenem Sync
            }

            RemoteOnOff.onNewValidData(Databroker.results_u32[4]/* 1.7.0 */, Databroker.results_u32[5] /* 2.7.0 */);

            // TODO(anyone): Refactor that "special values" from main.cpp
            dow = l_result.time.tm_wday; // dow: 1..7 (1=MON...7=SUN)
            if (dow == 0) {              // tm_wday: days since Sunday (0...6)
                dow = 7;
            }
            mon = l_result.time.tm_mon + 1; // mon: 1...12  -   tm_mon: 0...11 (months since January)
            myyear = l_result.time.tm_year - 100; // myyear: Jahr nur 2 stellig benötigt (seit 2000)

            if (!_readerIsOnline || msNow - _lastTimeSync > 1800000u ) {
                if (!_readerIsOnline) {
                    LOG_IP("Data synced with counter");
                    _readerIsOnline = true;
                }
                // Sync time afer sync with reader or each 30 minutes avoid internal clock drift
                setTime(Databroker.ts);
                _lastTimeSync = msNow;
            }

            _stateLastSetMs = msNow;
            _stateTimoutMs = 5000;  // ab jetzt: Timeout mit 10 Sekunden (2 * 5000)
            _stateErrorCnt = 0;
            _stateErrorMax = 2;
        } else if (r < -7) {
            // das Zählertelegramm sah prinzipiell gut aus aber nach dem Entschlüsseln passten einige Werte nicht
            // ==> d.h.: vermutlich stimmt der AMIS-Key nicht !
            Databroker.valid = 1;
            LOGF_DP("Decrypting data failed: error=%d", r);
        } else {
            // Beim Zählertelegram stimmten schon die Eckdatenm (Header, Checksumme, usw nicht)
            // ==> d.h.: vermutlich ungültige Daten empfangen
            Databroker.valid = 0;
            LOGF_DP("Invalid data received: error=%d", r);
        }
        _state = readReaderCounters;

        // TODO(anyone): Refactor - Create events on changed data
        ModbusSmartmeterEmulation.setCurrentValues((bool)(Databroker.valid == 5),
                                                   Databroker.results_u32[4], Databroker.results_u32[5],
                                                   Databroker.results_u32[0], Databroker.results_u32[1]);
        ShellySmartmeterEmulation.setCurrentValues((bool)(Databroker.valid == 5),
                                                   Databroker.results_u32[4], Databroker.results_u32[5],
                                                   Databroker.results_u32[0], Databroker.results_u32[1]);
        ThingSpeak.onNewData((bool)(Databroker.valid == 5), &Databroker.results_u32[0], Databroker.ts);
        SYSTEMMONITOR_STAT();
     }
}

void AmisReaderClass::loop()
{
    uint32_t msNow;

    if (!_isEnabled) {
        return;
    }

    // always read some bytes from the serial interface
    pollSerial();

    msNow = millis();

    // Do all releated things querying serialnumber
    processStateSerialnumber(msNow);

    // Do all releated steps to read the counters
    processStateCounters(msNow);

    if ( msNow - _stateLastSetMs >= _stateTimoutMs ) {
        // Timeout: seems something gone at the actual state wrong and needed too long
        LOGF_DP("State timeout occured! current state=%d", (int)_state);

        _stateLastSetMs = msNow;
        _stateErrorCnt++;

        // Be sure to cleanup the serial interface
        _serialReadBufferIdx = 0;
        clearSerialRx();
        _bytesInBufferExpectd = 0;

        if (_state == waitForReaderSerial) {
            _state = requestReaderSerial;
        }
    }

    if (_stateErrorCnt >= _stateErrorMax) {
        LOGF_DP("State max errors reached! Switching state. current state=%d", (int)_state);

        if (_readerIsOnline) {
            LOGF_WP("Sync lost. %u data frames received. Starting resync...", _validFrameCnt);
            _readerIsOnline = false;
        }
        _validFrameCnt = 0;
        Databroker.valid = 0;
        ModbusSmartmeterEmulation.setCurrentValues(false);
        ThingSpeak.onNewData(false);

        _stateErrorCnt = 0;

        _bytesInBufferExpectd = 0;

        _serialReadBufferIdx = 0;
        clearSerialRx();

        if (_state == initReadSerial || _state == requestReaderSerial ||
            _state == waitForReaderSerial || _state == decodeReaderSerial ) {
            if (_readSerialNumberMode == tryRead || _readSerialNumberMode == disabled ) {
                // we tried to get the serialnumber ... skip that - get the counter numbers
                // btw: "disabled" should never happend here
                _state = initReadCounters;
            } else {
                _state = initReadSerial;
            }
        } else {
            // we tried to get the counter numbers ... do a fresh start by first querying serial number
            if (_readSerialNumberMode == tryRead || _readSerialNumberMode == mustRead ) {
                _state = initReadSerial;
            } else {
                _state = initReadCounters;
            }
        }
    }
}

void AmisReaderClass::setKey(const char *key)
{
    size_t i=0;
    for (size_t j=0; j < strlen(key)-1; i++, j+=2) {
        if (i >= sizeof(_key)) {
            break;
        }
        _key[i] = Utils::hexchar2Num(key[j]);
        _key[i] <<= 4;
        _key[i] |= Utils::hexchar2Num(key[j+1]);
    }
    while (i < sizeof(_key)) {
        _key[i++] = 0; // fill all the rest with 0
    }
    AES128_set_key(_key);
}

void AmisReaderClass::setKey(const uint8_t *key)
{
    memcpy(_key, key, sizeof(_key));
    AES128_set_key(_key);
}

void AmisReaderClass::enable() {
    _isEnabled = true;
}

#if 0
void AmisReaderClass::enableRestart() {
    if (_isEnabled) {
        return;
    }
    _isEnabled = true;
    _state = initReadSerial;
}
#endif

void AmisReaderClass::disable() {
    _isEnabled = false;
}

AmisReaderClass AmisReader;

/* vim:set ts=4 et: */
