#include "proj.h"

#include "LedSingle.h"

const char PAGE_upgrade[] PROGMEM = R\
"=====(
<!doctype html>
<html lang="de" style="font-family:Arial;">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1" />
<h1>Version Upgrade</h1><br>
Upgrade vervollständigen: im Filedialog bitte 'littlefs.bin' auswählen.<br><br><br>
<form method='POST' action='/update' enctype='multipart/form-data' id="up">
  <input type='file' name='update'><input type='button' value='Update' onclick="btclick();">
</form>
<br><div id="wait"></div>
</html>
<script>
function btclick() {
  document.getElementById("up").submit();
  document.getElementById("wait").innerHTML='Bitte warten,das dauert jetzt etwas länger...';
}
</script>
)=====";


struct strConfig2 {
  String ssid;
  String password;
  byte  IP[4];
  byte  Netmask[4];
  byte  Gateway[4];
  byte Nameserver[4];
  boolean dhcp;
  uint8_t rfpower;
  bool thingspeak_aktiv;
};
strConfig2 config2;

String  ReadStringFromEEPROM(int beginaddress) {
  byte counter=0;
  char rChar;
  String retString = "";
  while(1) {
    rChar = EEPROM.read(beginaddress + counter);
    if(rChar == 0) break;
    if(counter > 31) break;
    counter++;
    retString.concat(rChar);

  }
  return retString;
}

boolean ReadConfig() {
  if(EEPROM.read(0) == 'C' && EEPROM.read(1) == 'F'  && EEPROM.read(2) == 'G') {
    config2.Nameserver[0] = EEPROM.read(12);
    config2.Nameserver[1] = EEPROM.read(13);
    config2.Nameserver[2] = EEPROM.read(14);
    config2.Nameserver[3] = EEPROM.read(15);

    config2.dhcp =   EEPROM.read(16);
    config2.rfpower  = EEPROM.read(26);

    config2.IP[0] = EEPROM.read(32);
    config2.IP[1] = EEPROM.read(33);
    config2.IP[2] = EEPROM.read(34);
    config2.IP[3] = EEPROM.read(35);
    config2.Netmask[0] = EEPROM.read(36);
    config2.Netmask[1] = EEPROM.read(37);
    config2.Netmask[2] = EEPROM.read(38);
    config2.Netmask[3] = EEPROM.read(39);
    config2.Gateway[0] = EEPROM.read(40);
    config2.Gateway[1] = EEPROM.read(41);
    config2.Gateway[2] = EEPROM.read(42);
    config2.Gateway[3] = EEPROM.read(43);

    config2.ssid = ReadStringFromEEPROM(62);
    config2.password = ReadStringFromEEPROM(94);
    return true;
  }
  else {
    return false;
  }
}

void ConfigureWifi(bool ap) {
  int i=40;
  int j=0;
  WiFi.disconnect();
  pinMode(14,INPUT);
  digitalWrite(14,HIGH);
  delay(10);
  if (digitalRead(14)==LOW || ap) {
      WiFi.disconnect();
      WiFi.mode(WIFI_AP);
      WiFi.softAP( "ESP8266_AMIS");
      #if DEBUGHW==2
      S.println(F("\r\n[ upgrade ] AP-Mode: 192.168.4.1"));
      #endif
      inAPMode=true;
      LedBlue.turnBlink(500, 500);
  }
  else {
    #if DEBUGHW==2                                     // Debug-Ausgaben nur über Serielle sinnvoll!
    S.print(F("[ upgrade ] Configuring Wifi:  "));
    if (config2.dhcp) S.println(F("[ upgrade ] DHCP"));
    #endif
    do {                  // Wait for connection
      WiFi.mode(WIFI_STA);
      for (int x=0; x < 500;++x) delay(1);
      #if DEBUGHW==2
      S.print(F("."));
      #endif
      if(++i>40) {
        #if DEBUGHW==2
        S.print(F("#"));
        #endif
          WiFi.disconnect();
        if (!config2.dhcp) {
          WiFi.config(IPAddress(config2.IP[0],config2.IP[1],config2.IP[2],config2.IP[3] ),
                      IPAddress(config2.Nameserver[0],config2.Nameserver[1],config2.Nameserver[2],config2.Nameserver[3] ),
                      IPAddress(config2.Gateway[0],config2.Gateway[1],config2.Gateway[2],config2.Gateway[3]),
                      IPAddress(config2.Netmask[0],config2.Netmask[1],config2.Netmask[2],config2.Netmask[3] ));
        }
        WiFi.begin(config2.ssid.c_str(), config2.password.c_str());
        i=0; j++;
      }
        if (j > 5) {ESP.wdtDisable(); while (1);} //ESP.restart();
    } while(WiFi.status() != WL_CONNECTED);
    WiFi.setOutputPower(config2.rfpower);  // 0..20.5 dBm
    LedBlue.turnBlink(4000, 10);
    #if DEBUGHW==2
    S.print(F("[ upgrade ] Connected to "));
    S.println(WiFi.SSID());
    S.print(F("[ upgrade ] IP address: "));
    S.println(WiFi.localIP());
    #endif
  }
}

void upgrade (bool save) {
  EEPROM.begin(256);
	if (!ReadConfig()) {
    ConfigureWifi(1);
    save=false;
	}
	else ConfigureWifi(0);
	if (save) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["dhcp"] = config2.dhcp;
    root["ip_gateway"]=String(config2.Gateway[0])+"."+String(config2.Gateway[1])+"."+String(config2.Gateway[2])+"."+String(config2.Gateway[3]);
    root["ip_nameserver"]=String(config2.Nameserver[0])+"."+String(config2.Nameserver[1])+"."+String(config2.Nameserver[2])+"."+String(config2.Nameserver[3]);
    root["ip_netmask"]=String(config2.Netmask[0])+"."+String(config2.Netmask[1])+"."+String(config2.Netmask[2])+"."+String(config2.Netmask[3]);
    root["ip_static"]=String(config2.IP[0])+"."+String(config2.IP[1])+"."+String(config2.IP[2])+"."+String(config2.IP[3]);
    root["ssid"]=config2.ssid;
    root["rfpower"]=config2.rfpower;
    root["wifipassword"]=config2.password;
    root["command"]="/config_wifi";
    File f = LittleFS.open("/config_wifi", "w+");
    if(f) {
      root.prettyPrintTo(f);
      f.close();
      writeEvent("INFO", "upgrade", "WiFi settings created form EEPROM", "");
      #if DEBUGHW==2
      S.print("[ upgrade ] config_wifi created from EEPROM\n");
      #endif
    }
	}
}
