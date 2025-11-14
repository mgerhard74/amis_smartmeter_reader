#include "proj.h"

#include "AmisReader.h"

/*
  Smartmeter Emulator TCP für Fronius Gen24. Stark vereinfacht auf das tatsächliche Fronius-Polling.
  Es wird nur der Wert AC-Power korrekt behandelt, der Rest ist Fake!
  Reicht aber für korrekte dyn. Einspeisebegrenzung.
*/
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


// Auf Big-Endian konvertierte Register-Var
const uint16_t PROGMEM BE_holdregs[]={                                                           // Zählerkennung:
/*00*/ 0x7553, 0x536e, 0x0100, 0x4100, 0x4100, 0x6d00, 0x6900, 0x7300, 0x2000, 0x5200,           // SunSA m i s   R
/*10*/ 0x6500, 0x6100, 0x6400, 0x6500, 0x7200, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,           // e a d e r
/*20*/ 0x5300, 0x6d00, 0x6100, 0x7200, 0x7400, 0x2000, 0x4d00, 0x6500, 0x7400, 0x6500,
/*30*/ 0x7200, 0x2000, 0x3600, 0x3300, 0x4100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
/*40*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
/*50*/ 0x0000, 0x0000, 0x3500, 0x3400, 0x3400, 0x3300, 0x3300, 0x3200, 0x3200, 0x3100,           // Ser-Nr
/*60*/ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xf000, 0xd500,
/*70*/ 0x7c00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

uint8_t mBuffer[182];        // für die Zähler-Daten
uint8_t mHeader[12];         // Header Rx + Tx
typedef union {              // für Float BigEndian wandeln
    unsigned int intval;
    float value;
    uint8_t bytes[sizeof(float)];
} UFB;
UFB floatvar;
bool buffered;
AsyncServer* meter_server;

 /* clients events */
static void handleError(void* arg, AsyncClient* client, int8_t error) {
	//eprintf("[Fronius] connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}

static void handleDisconnect(void* arg, AsyncClient* client) {
	//eprintf("[Fronius] client %s disconnected \n", client->remoteIP().toString().c_str());
}

static void handleTimeOut(void* arg, AsyncClient* client, uint32_t time) {
	//eprintf("[Fronius] client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}

static void handleData(void* arg, AsyncClient* client, void *data, size_t len);

static void handleNewClient(void* arg, AsyncClient* client) {
	eprintf("[Fronius] new client has been connected to server, ip: %s\n", client->remoteIP().toString().c_str());

	// register events
	client->onData(&handleData, NULL);
	client->onError(&handleError, NULL);
	client->onDisconnect(&handleDisconnect, NULL);
	client->onTimeout(&handleTimeOut, NULL);
}

static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
  //eprintf("[Fronius] Poll IP:%s\n",client->remoteIP().toString().c_str());
  if (valid!=5) return;               // erst beantworten wenn Zählerdaten vorhanden
	memcpy(mHeader,data,len);
  uint16_t reg_idx=(mHeader[8]<<8) | mHeader[9];
  uint16_t reg_len=(mHeader[10]<<8) | mHeader[11];
//	eprintf("[Fronius] RegIdx:%d RegLen:%02d Dta:",reg_idx,reg_len);
//	for (unsigned i=0; i< len;i++) eprintf("%02x ",mHeader[i]);	eprintf("\n");
  if ((reg_idx > 40197 || reg_idx <40000 ) || mHeader[7]!=3) {   // Anfrage außerhalb Register
//    eprintf("[Fronius] Err %u\n",reg_idx);
    mHeader[5]=3;        // Länge
    mHeader[7]=0x83;     // FC MSB gesetzt
    mHeader[8]=2;        // Fehlercode "nicht verfügbar"
    len=9;
    buffered=false;
    if (client->space() > len && client->canSend()) {
  		client->add((char*)mHeader,len);
      client->send();
    }
    return;
  }
  else {                         // gültige Anfrage:
    mHeader[8]=reg_len * 2;      // Header aufbereiten
    reg_idx-=40000;
    len=reg_len*2+3;
    mHeader[4]=len>>8;           // Länge Antwort
    mHeader[5]=len & 0xff;       // Länge Antwort bis Ende
    len=reg_len*2+9;
    if (!buffered) {
      memset(mBuffer,0,182);
      mBuffer[48]=0x42;                            // 40096: 50 Hz
      mBuffer[49]=0x48;
      buffered=true;
    }

    switch (reg_idx) {          // diese Adressen werden von Symo Geräten abgefragt
      case 71:                  // Anfragen an andere Adressen liefern Müll!
      case 97:
        // die Register 40072..40128 werden im sec-Takt gelesen, reg_len==58
        memset(mBuffer,0,116);
        signed int xsaldo;
        xsaldo=(a_result[4]-a_result[5]);           // 1.7.0 - 2.7.0 = Power
        floatvar.value=(float)(xsaldo);
        mBuffer[52]=(floatvar.bytes[3]);            // Power Big Endian korrekt kopieren auf P gesamt 40098
        mBuffer[53]=(floatvar.bytes[2]);
        mBuffer[54]=(floatvar.bytes[1]);
        mBuffer[55]=(floatvar.bytes[0]);

        //floatvar.value=(float)(xsaldo)/230;
        //mBuffer[0]=(floatvar.bytes[3]);             // Total AC Current
        //mBuffer[1]=(floatvar.bytes[2]);
        //mBuffer[1]=(floatvar.bytes[1]);
        //mBuffer[3]=(floatvar.bytes[0]);
        floatvar.value=(float)(xsaldo)/690;
        mBuffer[4]=(floatvar.bytes[3]);             // Phase A Current
        mBuffer[5]=(floatvar.bytes[2]);
        mBuffer[6]=(floatvar.bytes[1]);
        mBuffer[7]=(floatvar.bytes[0]);
        memcpy(&mBuffer[8], &mBuffer[4], 4);        // Phase B Current
        memcpy(&mBuffer[12], &mBuffer[4], 4);       // Phase C Current
        floatvar.value=230;
        mBuffer[16]=(floatvar.bytes[3]);            // Line to Neutral voltage
        mBuffer[17]=(floatvar.bytes[2]);
        mBuffer[18]=(floatvar.bytes[1]);
        mBuffer[19]=(floatvar.bytes[0]);
        memcpy(&mBuffer[20], &mBuffer[16], 4);      // Phase Voltage AN
        memcpy(&mBuffer[24], &mBuffer[16], 4);      // Phase Voltage BN
        memcpy(&mBuffer[28], &mBuffer[16], 4);      // Phase Voltage CN
        floatvar.value=400;
        mBuffer[32]=(floatvar.bytes[3]);            // Line to Line AC Voltage (average of active phases)
        mBuffer[33]=(floatvar.bytes[2]);
        mBuffer[34]=(floatvar.bytes[1]);
        mBuffer[35]=(floatvar.bytes[0]);
        memcpy(&mBuffer[36], &mBuffer[32], 4);      // Phase Voltage AB
        memcpy(&mBuffer[40], &mBuffer[32], 4);      // Phase Voltage BC
        memcpy(&mBuffer[44], &mBuffer[32], 4);      // Phase Voltage CA
        floatvar.value=(float)(xsaldo)/3;
        mBuffer[56]=(floatvar.bytes[3]);            // Watt Phase A
        mBuffer[57]=(floatvar.bytes[2]);
        mBuffer[58]=(floatvar.bytes[1]);
        mBuffer[59]=(floatvar.bytes[0]);
        memcpy(&mBuffer[60], &mBuffer[56], 4);      // Watt Phase B
        memcpy(&mBuffer[64], &mBuffer[56], 4);      // Watt Phase C
        floatvar.value=1;
        mBuffer[100]=(floatvar.bytes[3]);           // Power Factor Sum
        mBuffer[101]=(floatvar.bytes[2]);
        mBuffer[102]=(floatvar.bytes[1]);
        mBuffer[103]=(floatvar.bytes[0]);
        memcpy(&mBuffer[104], &mBuffer[100], 4);    // Power Factor Phase A
        memcpy(&mBuffer[108], &mBuffer[100], 4);    // Power Factor Phase B
        memcpy(&mBuffer[112], &mBuffer[100], 4);    // Power Factor Phase C
        mBuffer[48]=0x42;                           // 40096: 50 Hz
        mBuffer[49]=0x48;
        break;

      // die Register 40130..40160 werden jede Minute gelesen, reg_len==32
      case 129:
        floatvar.value=((float)(a_result[1]));       // 2.8.0
        //eprintf("E %f %u %x %x %x %x\n",floatvar.value,a_result[1],floatvar.bytes[3],floatvar.bytes[2],floatvar.bytes[1],floatvar.bytes[0]);
        mBuffer[116]=floatvar.bytes[3];             // Energy  Big Endian korrekt kopieren auf export 40130
        mBuffer[117]=floatvar.bytes[2];
        mBuffer[118]=floatvar.bytes[1];
        mBuffer[119]=(floatvar.bytes[0]);

        floatvar.value=((float)(a_result[0]));      // 1.8.0
        mBuffer[132]=(floatvar.bytes[3]);          // Energy  Big Endian korrekt kopieren auf import  40138
        mBuffer[133]=(floatvar.bytes[2]);
        mBuffer[134]=(floatvar.bytes[1]);
        mBuffer[135]=(floatvar.bytes[0]);
        break;

      case 195:                                   // end-block, Abfrage nur 1x bei Start, reg_len==2
        mBuffer[0]=0xff;
        mBuffer[1]=0xff;
        reg_idx=193;                              // so tun als ob...
        buffered=false;
    }
  }

	// reply to client
	if (client->space() > len && client->canSend()) {
    int buffIdx;
		client->add((char*)mHeader, 9);   // MBAP Header
		if (reg_idx < 71) {               // Allg. Daten aus ROM
      memcpy_P(mBuffer,&BE_holdregs[reg_idx],reg_len*2);
      buffIdx=0;
      buffered=false;
		} else {                          // elektr. Zählerdaten
		  if (reg_idx==193){              // Meter-Flags, immer 0, Daten vom mBuffer-Anfang nehmen; reg_len==2
        buffIdx=0;
		  }
      else buffIdx=(reg_idx-71)*2;    // Addr. gewünschter Daten
		}
    client->add((char*)(&mBuffer[buffIdx]), len-9);
		client->send();
	}
}

void meter_init() {
	meter_server = new AsyncServer(502); // start listening on tcp port 502
	meter_server->onClient(&handleNewClient, meter_server);
	meter_server->begin();
}
