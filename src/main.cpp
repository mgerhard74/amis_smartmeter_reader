#include "ProjectConfiguration.h"

#include "proj.h"
//#define DEBUG
#include "debug.h"


#include "AmisReader.h"
#include "Exception.h"
#include "FileBlob.h"
#include "LedSingle.h"
#include "ModbusSmartmeterEmulation.h"
#include "Mqtt.h"
#include "Network.h"
#include "Reboot.h"
#include "RebootAtMidnight.h"
#include "RemoteOnOff.h"
#include "ThingSpeak.h"
#include "Utils.h"
#include "WatchdogPing.h"
#include "Webserver.h"


extern const char *__COMPILED_DATE_TIME_UTC_STR__;
extern const char *__COMPILED_GIT_HASH__;
extern const char *__COMPILED_GIT_BRANCH__;

#if DEBUGHW==1
    WiFiServer dbg_server(10000);
    WiFiClient dbg_client;
#endif
#ifdef STROMPREIS
String strompreis="";
#endif // strompreis


extern void historyInit();

static void secTick();

Ticker secTicker;
bool new_data_for_websocket;
unsigned first_frame=0;
static uint8_t dow_local;
static uint8_t mon_local;
unsigned kwh_day_in[7];
unsigned kwh_day_out[7];
unsigned last_mon_in;
unsigned last_mon_out;
uint32_t clientId;
int logPage=-1;
uint8_t updates;
String latestYYMMInHistfile;
#if DEBUGHW>0
  char dbg[128];
  String dbg_string;
#endif // DEBUGHW
kwhstruct kwh_hist[7];
bool doSerialHwTest=false;

// Funktion __get_adc_mode() ( mittels Macro ADC_MODE() ) muss hier definiert werden,
// ansonsten liefert ESP.getVcc() später keine gültigen Werte
ADC_MODE(ADC_VCC);


void setup() {
    Serial.begin(115200, SERIAL_8N1); // Setzen wir ggf fürs debgging gleich mal einen default Wert

    #if DEBUGHW==2
        #if DEBUG_OUTPUT==0
            Serial.begin(115200);
        #elif DEBUG_OUTPUT==1
            Serial1.begin(115200);
        #endif
    #endif // DEBUGHW

#ifdef AP_PIN
    pinMode(AP_PIN, INPUT_PULLUP);
    // pinMode(AP_PIN, INPUT); digitalWrite(AP_PIN, HIGH);
#endif

    // Start filesystem early - so we can do some logging
    LittleFS.begin();

    // Log some booting information
    writeEvent("INFO", "sys", "System starting...", "");
    writeEvent("INFO", "sys", "  " APP_NAME " Version", VERSION);
    writeEvent("INFO", "sys", "  Compiled [UTC]", __COMPILED_DATE_TIME_UTC_STR__);
    writeEvent("INFO", "sys", "  Git branch", __COMPILED_GIT_BRANCH__);
    writeEvent("INFO", "sys", "  Git version/hash", __COMPILED_GIT_HASH__);
    writeEvent("INFO", "sys", "  Reset reason", ESP.getResetReason());

    // Sichern des letzten Crashes
    Exception_DumpLastCrashToFile();

    // Set timezone to CET/CEST
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // Mal die config laden
    Config.init();
    Config.loadConfigGeneral();

    // im Mqtt.init() wird die mqtt-config geladen
    Mqtt.init();

    // Extraktion der Files für den Webserver vorbereiten
    // Alle Dateien zu extrahieren dauert zu lange (HardwareWatchdok wir d ausgelöst)
    // Deswegen passiert die eigentlich extraktion dann erst in der loop()
    FileBlobs.init();
    FileBlobs.checkIsChanged();

    // Starten wir mal den AMIS-Reader
    AmisReader.init(AMISREADER_SERIAL_NO);  // Init mit Serieller Schnittstellennummer
    AmisReader.enable(); // und gleich enablen

    Reboot.init();

    Config.applySettingsConfigGeneral();

  // Start Network
#ifdef AP_PIN
    Network.init(digitalRead(AP_PIN) == LOW);
#else
    Network.init(false);
#endif
    NetworkConfigWifi_t networkConfigWifi = Network.getConfigWifi();
    Network.connect();

    // Load history of last 7 days and get YYMM of last entry in
    historyInit();

    // Webserver ... damit wir auch was machen können
    Webserver.init();    // Unter "/"" wird die "/index.html" ausgeliefert, "/update" ist eine statische fixe Seite

    Webserver.setCredentials(Config.use_auth, Config.auth_user, Config.auth_passwd);
    Webserver.setTryGzipFirst(Config.webserverTryGzipFirst); // webserverTryGzipFirst sollte hier true sein (lesen wir nicht aus der config)

    // Smart Meter Simulator
    ModbusSmartmeterEmulation.init();
    if (Config.smart_mtr) {
        ModbusSmartmeterEmulation.enable();
    }

    // initiate ping watchdog
    WatchdogPing.init();
    WatchdogPing.config(networkConfigWifi.pingrestart_ip.c_str(), networkConfigWifi.pingrestart_interval, networkConfigWifi.pingrestart_max);
    if (networkConfigWifi.pingrestart_do) {
        WatchdogPing.enable();
        if (Config.log_sys) {
            writeEvent("INFO", "wifi", "Ping restart check enabled", "");
        }
    }

    // Netzwerksteckdose (On/Off via Netzwerk)
    RemoteOnOff.init();
    RemoteOnOff.config(Config.switch_url_on, Config.switch_url_off, Config.switch_on, Config.switch_off, Config.switch_intervall);
    RemoteOnOff.enable();

    // ThingSpeak Datenupload
    ThingSpeak.init();
    ThingSpeak.setInterval(Config.thingspeak_iv);
    ThingSpeak.setApiKeyWriite(Config.write_api_key);
    ThingSpeak.setEnabled(Config.thingspeak_aktiv);

    // Reboot um Mitternacht?
    RebootAtMidnight.init();
    RebootAtMidnight.config();
    if (Config.reboot0) {
        RebootAtMidnight.enable();
    }

    secTicker.attach_scheduled(1, secTick);

    if (Config.log_sys) {
        writeEvent("INFO", "sys", "System setup completed, running", "");
    }
}

void loop() {
#if DEBUGHW==1
    if (dbg_string.length()) {          // Debug-Ausgaben TCP
        dbg_string+="\n";
        if (!dbg_client.connected()) dbg_client.stop();
        if (!dbg_client) dbg_client = dbg_server.available();
        if (dbg_client)  dbg_client.print(dbg_string);
        dbg_string="";
    }
#elif DEBUGHW==2
    if (dbg_string.length()) {          // Debug-Ausgaben Serial
        S.print(dbg_string);
        dbg_string="";
    }
#elif DEBUGHW==3
    if (dbg_string.length()) {          // Debug-Ausgaben Websock
        ws.text(clientId,dbg_string);
        //Serial1.println(dbg_string);
        dbg_string="";
    }
#endif

    FileBlobs.loop();

    Reboot.loop();

    if (ws->count()) {      // ws-connections
        if (new_data_for_websocket) {
            new_data_for_websocket=false;
            sendZData();
        }
    }

    AmisReader.loop();  // Zähler auslesen

    if (logPage >=0) {
        sendEventLog(clientId,logPage);
        logPage=-1;
    }

    LedBlue.loop();
    WatchdogPing.loop();

    if (doSerialHwTest) {
        for (unsigned i=0; i < 255; i++)  {
            Serial.write(i);
            delay(1);
          }
        Serial.flush();
        doSerialHwTest = false;
    }
}


static void writeHistFileIn(int x, uint32_t val) {
    DBGOUT("hist_in "+String(x)+" "+String(val)+"\n");
    File f = LittleFS.open("/hist_in"+String(x), "w");
    if (f) {
        f.print(val);
        f.close();
    }
}

static void writeHistFileOut(int x, uint32_t val) {
    DBGOUT("hist_out "+String(x)+" "+String(val)+"\n");
    File f = LittleFS.open("/hist_out"+String(x), "w");
    if (f) {
        f.print(val);
        f.close();
    }
}


static String appendToMonthFile(uint8_t yy, uint8_t mm, uint32_t v_1_8_0, uint32_t v_2_8_0)
{
// Hängt die ersten in Monat verfügbaren Zählerstände (1.8.0 und 2.8.0)
// einfach an die Datei an.
#if 1
    size_t len;
    char dataLine[28];
    //     4  1   10   1  10   1 1   = 28
    //   "yymm  NUMBER1 NUMBER2\n\0"
    len = snprintf(dataLine, sizeof(dataLine), "%02u%02u %u %u\n", yy, mm, v_1_8_0, v_2_8_0); // 1.8.0, 2.8.0 = Zählerstände Verbrauch(Energie+) Lieferung(Energie-)*/

    File f = LittleFS.open("/monate", "a");
    if (f) {
        f.write(dataLine, len);
        f.close();
    }
    dataLine[5] = 0;
    return String(dataLine);
#else
    String mm = "0" + String(m);
    if (s.length()<2) s="0"+s;
    s=String(y)+s;
    eprintf("F: %u %u %s", y, m, s.c_str());
    File f = LittleFS.open("/monate", "a");
    f.print(s+" ");
    f.print(v_1_8_0);
    f.print(" ");
    f.print(v_2_8_0);
    f.print('\n');          // f.println würde \r anfügen!
    f.close();
    return s;
#endif
}

static void secTick() {
    // wird jede Sekunde aufgerufen

    if (ws->count()) {        // ws-connections
        if (first_frame==0) {
            sendZDataWait();
        }
    }

    if (valid==5) {
        if (first_frame==3) { // 1. Zählerdatensatz nach reset
            first_frame=2;      // nächste action beim nächsten secTick
            int x=dow-2;        // gestern
            if (x < 0) x=6;
            if (x>6) x=0;
            if (kwh_day_in[x] ==0) {          // gestern noch keine Werte: momentanen Stand wegschreiben
                kwh_day_in[x]=a_result[0];      // 1.8.0 Bezug
                writeHistFileIn(x,a_result[0]);
            }
            if (kwh_day_out[x] ==0) {         // gestern noch keine Werte: momentanen Stand wegschreiben
                kwh_day_out[x]=a_result[1];     // 2.8.0 Lieferung
                writeHistFileOut(x,a_result[1]);
            }
            dow_local=dow;
            String s=String(mon);
            if (s.length()<2) {
                s="0"+s;
            }
            s=String(myyear)+s;
            if (s.compareTo(latestYYMMInHistfile)!=0) {
                latestYYMMInHistfile = appendToMonthFile(myyear,mon, a_result[0], a_result[1]);  // Monat noch nicht im File
            }
            mon_local=mon;
        } else if (first_frame==2) {        // Wochentabelle Energie erzeugen
            int x = dow-2;                  // gestern, idx ab 0
            if (x < 0) {
                x = 6;
            }
            for (int i=0; i<7;i++) {
                int vg = x-1;
                if (vg < 0) {
                    vg = 6;
                }

                if ((kwh_day_in[x] > kwh_day_in[vg]) && kwh_day_in[vg]) {
                    kwh_hist[i].kwh_in = kwh_day_in[x] - kwh_day_in[vg];
                    kwh_hist[i].dow = x;
                } else {
                    kwh_hist[i].kwh_in = 0;
                }
                if ((kwh_day_out[x] > kwh_day_out[vg]) && kwh_day_out[vg]) {
                    kwh_hist[i].kwh_out = kwh_day_out[x] - kwh_day_out[vg];
                    kwh_hist[i].dow = x;
                } else {
                    kwh_hist[i].kwh_out = 0;
                }
                x--;
                if (x < 0) {
                    x = 6;
                }
            }
            updates = 3;                      // Trigger WebClients Update
            first_frame = 1;                  // Tabelle erzeugt, initialisierung abgeschlossen
        }

        if (dow_local != dow) {           // Tageswechsel, dow 1..7
            int x=dow-2;                    // gestern, idx ab 0
            if (x < 0) x=6;                 // x zeigt auf gestern
            kwh_day_in[x]=a_result[0];      // 1.8.0
            writeHistFileIn(x,a_result[0]);
            kwh_day_out[x]=a_result[1];     // 2.8.0
            writeHistFileOut(x,a_result[1]);
            dow_local=dow;
            if (mon_local != mon) {         // Monatswechsel
                latestYYMMInHistfile = appendToMonthFile(myyear, mon, a_result[0], a_result[1]);
                mon_local=mon;
            }
            first_frame=2;                  // Wochen- + Monatstabelle Energie neu erzeugen
        }
    }


    if (updates){
        switch (updates) {
        case 2:
            energieWeekUpdate();                   // Wochentabelle Energie senden
            break;
        case 1:
            energieMonthUpdate();                   // Wochentabelle Energie senden
            break;
        }
        updates--;
    }
}

void writeEvent(String type, String src, String desc, String data) {
    File eventlog = LittleFS.open("/eventlog.json", "a");
    if (!eventlog) {
        return;
    }

    if(eventlog.size() > 50000) {
        eventlog.close();
        LittleFS.remove("/eventlog.json");
        eventlog = LittleFS.open("/eventlog.json", "a");
        if (!eventlog) {
            return;
        }
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root[F("type")] = type;
    root[F("src")] = src;
    root[F("desc")] = desc;
    root[F("data")] = data;
    root[F("time")] = timecode;
    root.printTo(eventlog);
    eventlog.print("\n");
    eventlog.close();
}

/* vim:set ts=4 et: */
