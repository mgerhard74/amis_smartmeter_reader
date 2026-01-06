#include "ModbusSmartmeterEmulation.h"

#include "SystemMonitor.h"
#include "UA.h"
#include "unions.h"
#include "unused.h"

/*
  Smartmeter Emulator TCP für Fronius Gen24. Stark vereinfacht auf das tatsächliche Fronius-Polling.
  Es wird nur ein paar Werte korrekt behandelt, der Rest ist Fake!
  Reicht aber für korrekte dyn. Einspeisebegrenzung.
*/

// siehe https://www.fronius.com/en/~/downloads/Solar%20Energy/Operating%20Instructions/42,0410,2649.pdf
//       https://www.fronius.com/QR-link/0024
//       https://github.com/mgerhard74/amis_smartmeter_reader/issues/7#issuecomment-2295172259 bzw
//       https://github.com/user-attachments/files/16647659/Smart_Meter_Register_Map_Float.xlsx


///uint16_t holdregs[]= {
///*00*/  // 0,
///*01*/   21365, 28243,   // 0x5375  0x6e53                      // "SunsS"
///*03*/   1,
///*04*/   65,                                                    // 65 Register
//// hex   46  72  6f  6E  69  75  73
///*05*/   70,114,111,110,105,117,115,0,0,0,0,0,0,0,0,0,          // Manufacturer "Fronius"
///*21*/   83,109,97,114,116,32,77,101,116,101,114,32,54,51,65,0, // Device Model "Smart Meter 63A"
///*37*/   0,0,0,0,0,0,0,0,                                       // Options N/A
///*45*/   0,0,0,0,0,0,0,0,                                       // SW-Vers. N/A
///*53*/   48,48,48,48,48,48,49,49,0,0,0,0,0,0,0,0,               // Serial Number: 00000001 (49,54,50,50,48,49,49,56   muss unique sein per Symo
///*69*/   240,                                                   // Modbus TCP Address: 240
///*70*/   213,                                                   // 3-ph + Nulleiter,
///*71*/   124,                                                   // Länge Datenregister : 40195 ist letztes R
///*72*/   0,0,0,0,0,0,0,0,0,0,     // AC Curr 1,2,3              //
///*82*/   0,0,0,0,0,0,0,0,0,0,     //
///*92*/   0,0,0,0,0,0,0,0,0,0,     // 96: Frequ 50.0 Hz 98:P ges, 100: P Phase 1
///*102*/  0,0,0,0,0,0,0,0,0,0,     // P Phase2 P Phase3
///*112*/  0,0,0,0,0,0,0,0,0,0,
///*122*/  0,0,0,0,0,0,0,0,0,0,     // 130:  Total Lieferung, 2.8.0
///*132*/  0,0,0,0,0,0,0,0,0,0,     // 138:  Total Bezug, 1.8.0
///*142*/  0,0,0,0,0,0,0,0,0,0,
///*152*/  0,0,0,0,0,0,0,0,0,0,
///*162*/  0,0,0,0,0,0,0,0,0,0,
///*172*/  0,0,0,0,0,0,0,0,0,0,
///*182*/  0,0,0,0,0,0,0,0,0,0,
///*192*/  0,0,0,0,
///*196*/  65535,                                             // End Mark,
///*197*/  0};                                                // nächster Block 0


// Auf Big-Endian konvertierte Register-Variablen für RegisterNummer 40001 bis 40071 (=RegisterIndex 40000 - 40070)
static uint16_t const PROGMEM BE_holdregs_40000[71] = {                                 // Zählerkennung:
/*00*/ 0x7553, 0x536e, 0x0100, 0x4100, 0x4100, 0x6d00, 0x6900, 0x7300, 0x2000, 0x5200,  // SunSA m i s   R
/*10*/ 0x6500, 0x6100, 0x6400, 0x6500, 0x7200, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // e a d e r
/*20*/ 0x5300, 0x6d00, 0x6100, 0x7200, 0x7400, 0x2000, 0x4d00, 0x6500, 0x7400, 0x6500,
/*30*/ 0x7200, 0x2000, 0x3600, 0x3300, 0x4100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
/*40*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
/*50*/ 0x0000, 0x0000, 0x3500, 0x3400, 0x3400, 0x3300, 0x3300, 0x3200, 0x3200, 0x3100,  // Ser-Nr
/*60*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xf000, 0xd500,
/*70*/ 0x7c00
/*71*/ // Index 40071 wird dann schon dynamisch vom Zähler versorgt
};

// Adressierung über Index (Beginnt bei 0)
#define SET_HOLD_REG_IDX(IDX, VALUE)  _modbus_reg_buffer[(IDX) - 40000] = VALUE
#if 1
// As the underlying CPU uses 4 bytes registers, try 4 bytes writes
#define SET_HOLD_REG_IDX_4BYTES(IDX, VALUE)  UA::WriteU32LE(&_modbus_reg_buffer[(IDX) - 40000], SWAP_LE4(VALUE))
#else
#define SET_HOLD_REG_IDX_4BYTES(IDX, VALUE)
            _modbus_reg_buffer[(IDX) - 40000] = (VALUE & 0xffff); \
            _modbus_reg_buffer[(IDX) - 40000 + 1] = (VALUE >> 16)
#endif

// Adressierung über Registernummer (Beginnt bei 1)
#define SET_HOLD_REG(REGNO, VALUE)  SET_HOLD_REG_IDX((REGNO)-1, VALUE)
#define SET_HOLD_REG_4BYTES(REGNO, VALUE)  SET_HOLD_REG_IDX_4BYTES((REGNO)-1, VALUE)

// Die RegisterIndex 40000 - 40070 haben wir im Speicher 'BE_holdregs_40000'
// Anscheinend werden folgende Registerindizes abgefragt:
//      40071 ... 40128  (reg_len=58) im Sekundentakt
//      40129 ... 40160  (reg_len=32) im Minutentakt
//      40195 ... 40196  (reg_len=2) beim Start
// Ist uns jetzt aber egal - wir bauen uns einen Registerpuffer für
// den Index 40000-40196  (= Register 40001-40197)



// Bsp einer Anfrage: 000a 0000 0006 01 03 9c40 0002
struct __attribute__((__packed__))  mbapHeaderRequest { // Aka (ModBus Application Protocol Header) ... evrything here is BE (BigEndian)
    uint16_t tarnsactionId;     // Offset  0 -- Es muss der selbe Wert der Anfrage in der Antwort zurückgesendet werden
    uint16_t protocolId;        // Offset  2 -- Protokoll ID: 0x0000=Modbus
    uint16_t length;            // Offset  4 -- Wieviele Bytes jetzt noch kommen
    uint8_t  unitId;            // Offset  6 -- ID des Gerätes, welches abgefragt werden soll
    uint8_t  functionCode;      // Offset  7 -- Funktionscode 0x03=Read Analog Output Holding Registers
    uint16_t firstRegisterIdx;  // Offset  8 -- Erstes zu lesender RegisterIndex
    uint16_t numOfRegisters;    // Offset 10 -- Anzahl der zu lesenden Register
};
static_assert(sizeof(struct mbapHeaderRequest) == 12);

struct __attribute__((__packed__))  mbapHeaderResponseFunc3Error { // Error Response Header
    uint16_t tarnsactionId;     // Offset  0 -- Es muss der selbe Wert der Anfrage in der Antwort zurückgesendet werden
    uint16_t protocolId;        // Offset  2 -- Protokoll ID: 0x0000=Modbus
    uint16_t length;            // Offset  4 -- Wieviele Bytes jetzt noch kommen
    uint8_t  unitId;            // Offset  6 -- Es muss der selbe Wert der Anfrage in der Antwort zurückgesendet werden
    uint8_t  functionCode;      // Offset  7 -- Funktionscode 0x03=Read Analog Output Holding Registers (High bit soll bei Fehler gesetzt werden)
    uint8_t  functionErrorCode; // Offset  8 -- ?
};
static_assert(sizeof(struct mbapHeaderResponseFunc3Error) == 9);

struct __attribute__((__packed__))  mbapHeaderResponseFunc3Result { // Response Header auf Anfragen vom Typ 3
    uint16_t tarnsactionId;     // Offset  0 -- Es muss der selbe Wert der Anfrage in der Antwort zurückgesendet werden
    uint16_t protocolId;        // Offset  2 -- Protokoll ID: 0x0000=Modbus
    uint16_t length;            // Offset  4 -- Wieviele Bytes jetzt noch kommen
    uint8_t  unitId;            // Offset  6 -- Es muss der selbe Wert der Anfrage in der Antwort zurückgesendet werden
    uint8_t  functionCode;      // Offset  7 -- Funktionscode 0x03=Read Analog Output Holding Registers
    uint8_t  length2;           // Offset  8 -- Anzahl der Bytes, die jetzt noch kommen (=lenght - 3)
    //uint16_t registerConeten[numOfRegisters / 2];
};
static_assert(sizeof(struct mbapHeaderResponseFunc3Result) == 9);


ModbusSmartmeterEmulationClass::ModbusSmartmeterEmulationClass()
    : _server(SMARTMETER_EMULATION_SERVER_PORT)
{
    _currentValues.dataAreValid = false;
}

bool ModbusSmartmeterEmulationClass::perpareBuffers()
{
    // Allocate and fill the register buffer with some fixed values

    U4ByteValues float2Bytes;

    if (_modbus_reg_buffer != nullptr) {
        return true; // already prepared !
    }

    _modbus_reg_buffer = (uint16_t *) malloc(sizeof(uint16_t) * MODBUS_SMARTMETER_EMULATION_NUM_OF_REGS);
    if (_modbus_reg_buffer == nullptr) {
        return false;
    }
    memset(_modbus_reg_buffer, 0, sizeof(uint16_t) * MODBUS_SMARTMETER_EMULATION_NUM_OF_REGS);
    memcpy_P(_modbus_reg_buffer, BE_holdregs_40000, sizeof(BE_holdregs_40000));

    memset(&_valuesInBuffer, 0, sizeof(_valuesInBuffer));   // _valuesInBuffer.v1_7_0 ....  _valuesInBuffer.v2_8_0 = 0

    float2Bytes.f = 230;  float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
    SET_HOLD_REG_4BYTES(40080, float2Bytes.ui32t);  // Line to Neutral voltage (=230V)
    SET_HOLD_REG_4BYTES(40082, float2Bytes.ui32t);  // Phase Voltage AN
    SET_HOLD_REG_4BYTES(40084, float2Bytes.ui32t);  // Phase Voltage BN
    SET_HOLD_REG_4BYTES(40086, float2Bytes.ui32t);  // Phase Voltage CN

    float2Bytes.f = 400;  float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
    SET_HOLD_REG_4BYTES(40088, float2Bytes.ui32t);  // Line to Line AC Voltage (average of active phases) (=400V)
    SET_HOLD_REG_4BYTES(40090, float2Bytes.ui32t);  // Phase Voltage AB
    SET_HOLD_REG_4BYTES(40092, float2Bytes.ui32t);  // Phase Voltage BC
    SET_HOLD_REG_4BYTES(40094, float2Bytes.ui32t);  // Phase Voltage CA

    float2Bytes.f = 50;  float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
    SET_HOLD_REG_4BYTES(40096, float2Bytes.ui32t);  // Frequency (=50Hz)

    float2Bytes.f = 1;  float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
    SET_HOLD_REG_4BYTES(40122, float2Bytes.ui32t);  // Power Factor Sum
    SET_HOLD_REG_4BYTES(40124, float2Bytes.ui32t);  // Power Factor Phase A
    SET_HOLD_REG_4BYTES(40126, float2Bytes.ui32t);  // Power Factor Phase B
    SET_HOLD_REG_4BYTES(40128, float2Bytes.ui32t);  // Power Factor Phase C

    //SET_HOLD_REG_4BYTES(40194, 0x00000000);         // Meter Event Flags - set to 0

    SET_HOLD_REG(40196, 0xffff);                    // Identifies this as End block

    //SET_HOLD_REG(40197, 0x0000);                    // Length of model block

    return true;
}

void ModbusSmartmeterEmulationClass::init()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    _server.onClient(std::bind(&ModbusSmartmeterEmulationClass::clientOnClient, this, _1, _2), NULL);
}


void ModbusSmartmeterEmulationClass::setCurrentValues(bool dataAreValid, uint32_t v1_7_0, uint32_t v2_7_0, uint32_t v1_8_0, uint32_t v2_8_0)
{
    if (!_enabled) {
        return; // Don't waste time if there is nothing to do
    }
    _currentValues.dataAreValid = dataAreValid;
    if (!dataAreValid) {
        return; // Don't waste any more time
    }
    _currentValues.v1_7_0 = v1_7_0;
    _currentValues.v2_7_0 = v2_7_0;
    _currentValues.v1_8_0 = v1_8_0;
    _currentValues.v2_8_0 = v2_8_0;
    if (_clientsConnectedCnt == 0) {
        return; // Don't waste time if there is no client
    }

    U4ByteValues float2Bytes;
#if 0
    // Testing with some fixed values
    v1_7_0 = 1000;  v2_7_0 = 1234;
    v1_8_0 = 123456789u;  v2_8_0 = 987654321u;
#endif

    if (_valuesInBuffer.v1_7_0 != v1_7_0 || _valuesInBuffer.v2_7_0 != v2_7_0) {
        _valuesInBuffer.v1_7_0 = v1_7_0;
        _valuesInBuffer.v2_7_0 = v2_7_0;

        float saldo;
        saldo = (float) v1_7_0 - (float) v2_7_0;                    // 1.7.0 - 2.7.0 = Power

        //float2Bytes.f = (float)(saldo) / 230;                     // Total AC Current
        //float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
        //SET_HOLD_REG_4BYTES(40072, float2Bytes.ui32t);

        float2Bytes.f = saldo / 690;
        float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
        SET_HOLD_REG_4BYTES(40074, float2Bytes.ui32t);              // Phase A Current
        SET_HOLD_REG_4BYTES(40076, float2Bytes.ui32t);              // Phase B Current
        SET_HOLD_REG_4BYTES(40078, float2Bytes.ui32t);              // Phase C Current

        float2Bytes.f = saldo;
        float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
        SET_HOLD_REG_4BYTES(40098, float2Bytes.ui32t);              // Aktuelles Saldo = PowerGesamt

        float2Bytes.f = saldo / 3;
        float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
        SET_HOLD_REG_4BYTES(40100, float2Bytes.ui32t);              // Phase A
        SET_HOLD_REG_4BYTES(40102, float2Bytes.ui32t);              // Phase B
        SET_HOLD_REG_4BYTES(40104, float2Bytes.ui32t);              // Phase C
    }

    if (_valuesInBuffer.v2_8_0 != v2_8_0) {
        _valuesInBuffer.v2_8_0 = v2_8_0;
        float2Bytes.f = v2_8_0;
        float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
        SET_HOLD_REG_4BYTES(40130, float2Bytes.ui32t);              // Total Real Energy Exported
        // Todo: Eventuell die Register 40132, 40134 und 40136 auch versorgen?
    }

    if (_valuesInBuffer.v1_8_0 != v1_8_0) {
        _valuesInBuffer.v1_8_0 = v1_8_0;
        float2Bytes.f = v1_8_0;
        float2Bytes.ui32t = SWAP_BE4(float2Bytes.ui32t);
        SET_HOLD_REG_4BYTES(40138, float2Bytes.ui32t);              // Total Real Energy Imported
        // Todo: Eventuell die Register 40140, 40142 und 40144 auch versorgen?
    }
}

void ModbusSmartmeterEmulationClass::clientOnClient(void* arg, AsyncClient* client)
{
    UNUSED_ARG(arg);
    //eprintf("[Fronius] new client has been connected to server, ip: %s\n", client->remoteIP().toString().c_str());
    if (_clientsConnectedCnt++ == 0) {
        // First client connected so prepare internal Modbus register buffer with our actual values
        setCurrentValues(_currentValues.dataAreValid,
                         _currentValues.v1_7_0, _currentValues.v2_7_0,
                         _currentValues.v1_8_0, _currentValues.v2_8_0);
    }

    // register events
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;

    //_server.onClient(&clientOnClient, NULL);
    client->onData(std::bind(&ModbusSmartmeterEmulationClass::clientOnData, this, _1, _2, _3, _4));
    client->onError(std::bind(&ModbusSmartmeterEmulationClass::clientOnError, this, _1, _2, _3));
    client->onDisconnect(std::bind(&ModbusSmartmeterEmulationClass::clientOnDisconnect, this, _1, _2));
    client->onTimeout(std::bind(&ModbusSmartmeterEmulationClass::clientOnTimeOut, this, _1, _2, _3));
}

bool ModbusSmartmeterEmulationClass::setEnabled(bool enabled)
{
    if (enabled) {
        enable();
    } else {
        disable();
    }
    return _enabled;
}

bool ModbusSmartmeterEmulationClass::enable(void)
{
    if(_enabled) {
        return true;
    }
    _enabled = perpareBuffers();
    if(_enabled) {
        _server.begin();
    }
    return _enabled;
}

void ModbusSmartmeterEmulationClass::disable(void)
{
    if(!_enabled) {
        return;
    }
    _enabled = false;
    _server.end();
}

/* --------------------------- clients events --------------------------- */
void ModbusSmartmeterEmulationClass::clientOnError(void* arg, AsyncClient* client, int8_t error)
{
    UNUSED_ARG(arg);
    UNUSED_ARG(client);
    UNUSED_ARG(error);
    //eprintf("[Fronius] connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}

void ModbusSmartmeterEmulationClass::clientOnDisconnect(void* arg, AsyncClient* client)
{
    UNUSED_ARG(arg);
    UNUSED_ARG(client);
    //eprintf("[Fronius] client %s disconnected \n", client->remoteIP().toString().c_str());
    if (_clientsConnectedCnt) {
        _clientsConnectedCnt--;
    }
}

void ModbusSmartmeterEmulationClass::clientOnTimeOut(void* arg, AsyncClient* client, uint32_t time)
{
    UNUSED_ARG(arg);
    UNUSED_ARG(client);
    UNUSED_ARG(time);
    //eprintf("[Fronius] client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}

void writeEvent(String type, String src, String desc, String data);
void ModbusSmartmeterEmulationClass::clientOnData(void* arg, AsyncClient* client, void *data, size_t len)
{
    UNUSED_ARG(arg);
    //eprintf("[Fronius] Poll IP:%s\n",client->remoteIP().toString().c_str());
    if (!_currentValues.dataAreValid) {
        // nur beantworten wenn gültige Zählerdaten vorhanden
        client->close(false);
        return;
    }
    if (len < sizeof(struct mbapHeaderRequest)) {
        // Anfrage war nicht lang genug!

        // Weiter unten in der Funktion gehen wir immer davon aus, dass wir den Platz für 'mbapHeaderResponseFunc3Error' haben
        // Hier validieren, dass wir zumindest auch den Platz für den Error-Header haben
        static_assert(sizeof(struct mbapHeaderRequest) >= sizeof(struct mbapHeaderResponseFunc3Error));

        client->close(false);
        return;
    }
    if (!client->canSend() || client->space() < sizeof(struct mbapHeaderResponseFunc3Result)) {
        // Den Platz für den Antwortheader benötigen wir auf jeden Fall!
        client->close(false);
        return;
    }

    // Wir verwenden direkt die 'data' als Puffer für den Antwortheader
    mbapHeaderRequest *mbap_i = (mbapHeaderRequest *)data;
    if (UA::ReadU16LE(&mbap_i->protocolId) != 0x0000) {
        // Keine MODBUS Anfrage
        client->close(false);
        return;
    }

    if (_unitId <= 255 && mbap_i->unitId != _unitId) {
        // Nicht die erwartete GeräteID
        client->close(false);
        return;
    }

    if (mbap_i->functionCode != 3) { // Funktionscode 3 = 'Read Analog Output Holding Registers' ?
        struct mbapHeaderResponseFunc3Error *mbap_e = (mbapHeaderResponseFunc3Error *) data;
        UA::WriteU16BE(&mbap_e->length, 3); // es kommen noch 3 Bytes
        mbap_e->functionCode |= 0x80;       // Funktionscode: MSB setzen
        mbap_e->functionErrorCode = 1;      // Fehlercode "The received function code is not supported"
        client->add((char*)mbap_e, sizeof(struct mbapHeaderResponseFunc3Error));
        client->send();
        return;
    }

    uint16_t reg_idx_begin = UA::ReadU16BE(&mbap_i->firstRegisterIdx);
    uint16_t reg_cnt = UA::ReadU16BE(&mbap_i->numOfRegisters);
    uint16_t reg_idx_last = reg_idx_begin + reg_cnt - 1;

# if 0
    // Logging des headers
    {
        char msg[62];
        snprintf(msg, sizeof(msg),
                 "MBAP Header(%d):"      // 14B + %d
                 " %02x %02x %02x %02x"  // 4*3B
                 " %02x %02x %02x %02x"  // 4*3B
                 " %02x %02x %02x %02x", // 4*3B + 1B
            len,
            header[0], header[1], header[2], header[3],
            header[4], header[5], header[6], header[7],
            header[8], header[9], header[10], header[11]);
        writeEvent("INFO", "Modbus", msg, "");
    }
#endif

    //  eprintf("[Fronius] RegIdx:%d RegLen:%02d Dta:", reg_idx_begin, reg_cnt);
    //  char *mHeader = (char *)data;
    //  for (size_t i=0; i<len; i++) { eprintf("%02x ", mHeader[i]); } eprintf("\n");


// OUT_OF_UPPER_BOUNDS_HANDLE
//      Abfragen, die zuviele Register wollen werden
//      1... mit Fehler beendet
//      2... auf die noch vorhandenen zusammengeschnitten
#define OUT_OF_UPPER_BOUNDS_HANDLE 1


    if ( (reg_idx_begin < MODBUS_SMARTMETER_FIRST_AVAIL_REGIDX || reg_idx_begin > MODBUS_SMARTMETER_LAST_AVAIL_REGIDX )
#if (OUT_OF_UPPER_BOUNDS_HANDLE == 1)
        || (reg_idx_last > MODBUS_SMARTMETER_LAST_AVAIL_REGIDX)
#endif
        ) {
        // Anfrage außerhalb Register
        //    eprintf("[Fronius] Err idx:%u till %u (cnt=%u)\n", reg_idx_begin, reg_idx_last, reg_cnt);
        struct mbapHeaderResponseFunc3Error *mbap_e = (mbapHeaderResponseFunc3Error *) data;
        UA::WriteU16BE(&mbap_e->length, 3); // es kommen noch 3 Bytes
        mbap_e->functionCode |= 0x80;       // Funktionscode: MSB setzen
        mbap_e->functionErrorCode = 2;      // Fehlercode "The request attempted to access an invalid address"
        client->add((char*)mbap_e, sizeof(struct mbapHeaderResponseFunc3Error));
        client->send();
        return;
    }

#if (OUT_OF_UPPER_BOUNDS_HANDLE == 2)
    if (reg_idx_last > MODBUS_SMARTMETER_LAST_AVAIL_REGIDX) {
        reg_idx_last = MODBUS_SMARTMETER_LAST_AVAIL_REGIDX;
        reg_cnt = MODBUS_SMARTMETER_LAST_AVAIL_REGIDX + 1 - reg_idx_begin;
    }
#endif

    if ( reg_cnt > 127 ) {
        // Zuviele Register werden abgefragt ...
        // ... es muss ja in der Antwort length2(1 Byte) die Byteanzahl enthalten und die hätten
        //     dann nicht mehr in dem einen Byte Platz.
        //    eprintf("[Fronius] Err %u\n", reg_cnt);
        struct mbapHeaderResponseFunc3Error *mbap_e = (mbapHeaderResponseFunc3Error *) data;
        UA::WriteU16BE(&mbap_e->length, 3); // es kommen noch 3 Bytes
        mbap_e->functionCode |= 0x80;       // Funktionscode: MSB setzen
        mbap_e->functionErrorCode = 3;      // Fehlercode "The request had incorrect data."
        client->add((char*)mbap_e, sizeof(struct mbapHeaderResponseFunc3Error));
        client->send();
        return;
    }

    // Antwort senden

    // 1.) Genug Platz?
    if (client->space() < sizeof(struct mbapHeaderResponseFunc3Result) + reg_cnt * 2) {
        // Nein - zumindest noch Fehler zurückgeben
        struct mbapHeaderResponseFunc3Error *mbap_e = (mbapHeaderResponseFunc3Error *) data;
        UA::WriteU16BE(&mbap_e->length, 3); // es kommen noch 3 Bytes
        mbap_e->functionCode |= 0x80;       // Funktionscode: MSB setzen
        mbap_e->functionErrorCode = 4;      // Fehlercode "Slave Device Failure"
        client->add((char*)mbap_e, sizeof(struct mbapHeaderResponseFunc3Error));
        client->send();
        return;
    }

    // 2.) Header noch updaten
    struct mbapHeaderResponseFunc3Result *mbap_r = (mbapHeaderResponseFunc3Result *) data;
    UA::WriteU16BE(&mbap_r->length, reg_cnt*2 + 3); // Länge der Antwort in Bytes
    mbap_r->length2 = reg_cnt*2;                    // Anzahl der Bytes, die jetzt darauffolgend noch kommen

    // Header und Registerinhalt senden
    client->add((char*)mbap_r, sizeof(struct mbapHeaderResponseFunc3Result));
    client->add((char*)&_modbus_reg_buffer[reg_idx_begin - MODBUS_SMARTMETER_FIRST_AVAIL_REGIDX], reg_cnt * 2);
    client->send();

    SYSTEMMONITOR_STAT();
}

ModbusSmartmeterEmulationClass ModbusSmartmeterEmulation;

/* vim:set ts=4 et: */
