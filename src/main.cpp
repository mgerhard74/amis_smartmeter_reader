#include "ProjectConfiguration.h"

#include "proj.h"

#include "AmisReader.h"
#include "Application.h"
#include "Databroker.h"
#include "Exception.h"
#include "Failsafe.h"
#include "LedSingle.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_SYSTEM
#include "ModbusSmartmeterEmulation.h"
#include "ShellySmartmeterEmulation.h"
#include "Mqtt.h"
#include "Network.h"
#include "Reboot.h"
#include "RebootAtMidnight.h"
#include "RemoteOnOff.h"
#include "SystemMonitor.h"
#include "ThingSpeak.h"
#include "Utils.h"
#include "WatchdogPing.h"
#include "Webserver.h"
#include "__compiled_constants.h"


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
uint8_t updates;
String latestYYMMInHistfile;
kwhstruct kwh_hist[7];
bool doSerialHwTest=false;

// Funktion __get_adc_mode() ( mittels Macro ADC_MODE() ) muss hier definiert werden,
// ansonsten liefert ESP.getVcc() später keine gültigen Werte
ADC_MODE(ADC_VCC);

void setup() {
    /*
    // Reclaim 4KB for system use (required for WPS/Enterprise)
    // This results in loosing 4k heap for user context and giving system context(SYS) 4k more RAM
    //
    disable_extra4k_at_link_time();
    */

    Serial.begin(115200, SERIAL_8N1); // Setzen wir ggf fürs debgging gleich mal einen default Wert
   
    if(Failsafe.check()) {
        return;
    }

    Exception_InstallPostmortem(1);

#ifdef AP_PIN
    pinMode(AP_PIN, INPUT_PULLUP);
    // pinMode(AP_PIN, INPUT); digitalWrite(AP_PIN, HIGH);
#endif

    // Start filesystem early - so we can do some logging
    LittleFS.begin();

    Utils::init();

#ifdef AP_PIN
    Application.init(digitalRead(AP_PIN) == LOW);
#else
    Application.init(false);
#endif

    // Init logging
    Log.init("eventlog.json");
    Log.setLoglevel(CONFIG_LOG_DEFAULT_LEVEL, LOGMODULE_ALL);

    // Log some booting information
    LOG_PRINT_IP("System starting...");
    LOG_PRINT_IP("  " APP_NAME " Version " APP_VERSION_STR);
    LOG_PRINTF_IP("  Compiled [UTC] %s", __COMPILED_DATE_TIME_UTC_STR__);
    LOG_PRINTF_IP("  Git branch %s", __COMPILED_GIT_BRANCH__);
    LOG_PRINTF_IP("  Git version/hash %s", __COMPILED_GIT_HASH__);
    LOG_PRINT_IP("  PIO environment " PIOENV);
    //DOLOG_IP("  Reset reason %s", ESP.getResetReason().c_str());
    LOG_PRINTF_IP("  Reset info %s", ESP.getResetInfo().c_str());

    if (!Application.inAPMode()) {
        // Sichern des letzten Crashes ... auch das machen wir nur im "Normalbetrieb"
        Exception_DumpLastCrashToFile();
    }

    // Set timezone to CET/CEST
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // Minimalwerte von Stack, freies RAM, ... tracken
    SYSTEMMONITOR_STAT();

    // Mal die config laden
    Config.init();
    Config.loadConfigGeneral();

    if (!Config.log_sys) {
        Log.setLoglevel(LOGLEVEL_NONE, LOGMODULE_ALL);
    }

    // im Mqtt.init() wird die mqtt-config geladen
    Mqtt.init();

    // Starten wir mal den AMIS-Reader
    AmisReader.init(AMISREADER_SERIAL_NO);  // Init mit Serieller Schnittstellennummer
    AmisReader.enable(); // und gleich enablen

    Reboot.init();

    Config.applySettingsConfigGeneral();

    // Developer: Enable verbose logging on some modules
#if 0
    Log.setLoglevel(LOGLEVEL_VERBOSE, LOGMODULE_AMISREADER);
    Log.setLoglevel(LOGLEVEL_VERBOSE, LOGMODULE_MODBUS);
    Log.setLoglevel(LOGLEVEL_VERBOSE, LOGMODULE_MQTT);
    Log.setLoglevel(LOGLEVEL_VERBOSE, LOGMODULE_THINGSPEAK);
    Log.setLoglevel(LOGLEVEL_VERBOSE, LOGMODULE_WATCHDOGPING);
    Log.setLoglevel(LOGLEVEL_VERBOSE, LOGMODULE_REMOTEONOFF);
#endif

    // Start Network
    Network.init(Application.inAPMode());
    NetworkConfigWifi_t networkConfigWifi = Network.getConfigWifi();
    Network.connect();

    // Load history of last 7 days and get YYMM of last entry in
    historyInit();

    // Webserver ... damit wir auch was machen können
    Webserver.init();    // Unter "/"" wird die "/index.html" ausgeliefert, "/update" ist eine statische fixe Seite
    Webserver.reloadCredentials();

    // Modbus Smart Meter Emulator
    ModbusSmartmeterEmulation.init();
    if (Config.smart_mtr) {
        ModbusSmartmeterEmulation.enable();
        LOG_DP("ModbusSmartmeterEmulation enabled");
    }

    // Shelly Smart Meter Emulator
    if (Config.shelly_smart_mtr_udp &&
        ShellySmartmeterEmulation.init(Config.shelly_smart_mtr_udp_device_index, Config.shelly_smart_mtr_udp_hardware_id_appendix, Config.shelly_smart_mtr_udp_offset))
    {
        ShellySmartmeterEmulation.enable();
    }


    // initiate ping watchdog
    WatchdogPing.init();
    WatchdogPing.config(networkConfigWifi.pingrestart_ip, networkConfigWifi.pingrestart_interval, networkConfigWifi.pingrestart_max);
    if (networkConfigWifi.pingrestart_do) {
        WatchdogPing.enable();
        LOG_DP("WatchdogPing enabled");
    }

    // Netzwerksteckdose (On/Off via Netzwerk)
    RemoteOnOff.init();
    RemoteOnOff.config(Config.switch_url_on, Config.switch_url_off, Config.switch_on, Config.switch_off, Config.switch_intervall);
    RemoteOnOff.enable();

    // ThingSpeak Datenupload
    ThingSpeak.init();
    ThingSpeak.setInterval(Config.thingspeak_iv);
    ThingSpeak.setApiKeyWrite(Config.write_api_key);
    ThingSpeak.setEnabled(Config.thingspeak_aktiv);

    // Reboot um Mitternacht?
    RebootAtMidnight.init();
    RebootAtMidnight.config();
    if (Config.reboot0) {
        RebootAtMidnight.enable();
        LOG_DP("RebootAtMidnight enabled");
    }

    secTicker.attach_scheduled(1, secTick);

    SYSTEMMONITOR_STAT();

    LOG_IP("System setup completed, running");
}

void loop() {
    if (Failsafe.loop()) {
        return;
    }
    Reboot.loop();

    if (ws->count()) {      // ws-connections
        if (new_data_for_websocket) {
            new_data_for_websocket=false;
            sendZData();
        }
    }

    AmisReader.loop();  // Zähler auslesen

    Log.loop(); // Eine Seite des Logfiles an einen Websocket client senden

    LedBlue.loop();

    WatchdogPing.loop();

    MDNS.update();

    if (doSerialHwTest) {
        for (unsigned i=0; i < 255; i++)  {
            Serial.write(i);
            delay(1);
          }
        Serial.flush();
        doSerialHwTest = false;
    }

    SYSTEMMONITOR_STAT();
}


static void writeHistFileIn(int x, uint32_t val) {
    LOGF_VP("writeHistFileIn(): /hist_in%d = %" PRIu32, x, val);
    File f = LittleFS.open("/hist_in"+String(x), "w");
    if (f) {
        f.print(val);
        f.close();
    }
}

static void writeHistFileOut(int x, uint32_t val) {
    LOGF_VP("writeHistFileOut(): /hist_out%d = %" PRIu32, x, val);
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

    if (Databroker.valid==5) {
        if (first_frame==3) { // 1. Zählerdatensatz nach reset
            first_frame=2;      // nächste action beim nächsten secTick
            int x=dow-2;        // gestern
            if (x < 0) x=6;
            if (x>6) x=0;
            if (kwh_day_in[x] ==0) {          // gestern noch keine Werte: momentanen Stand wegschreiben
                kwh_day_in[x] = Databroker.results_u32[0];      // 1.8.0 Bezug
                writeHistFileIn(x, Databroker.results_u32[0]);
            }
            if (kwh_day_out[x] ==0) {         // gestern noch keine Werte: momentanen Stand wegschreiben
                kwh_day_out[x] = Databroker.results_u32[1];     // 2.8.0 Lieferung
                writeHistFileOut(x, Databroker.results_u32[1]);
            }
            dow_local=dow;
            String s=String(mon);
            if (s.length()<2) {
                s="0"+s;
            }
            s=String(myyear)+s;
            if (s.compareTo(latestYYMMInHistfile)!=0) {
                latestYYMMInHistfile = appendToMonthFile(myyear,mon, Databroker.results_u32[0], Databroker.results_u32[1]);  // Monat noch nicht im File
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
            kwh_day_in[x] = Databroker.results_u32[0];      // 1.8.0
            writeHistFileIn(x, Databroker.results_u32[0]);
            kwh_day_out[x] = Databroker.results_u32[1];     // 2.8.0
            writeHistFileOut(x, Databroker.results_u32[1]);
            dow_local=dow;
            if (mon_local != mon) {         // Monatswechsel
                latestYYMMInHistfile = appendToMonthFile(myyear, mon, Databroker.results_u32[0], Databroker.results_u32[1]);
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


/* vim:set ts=4 et: */
