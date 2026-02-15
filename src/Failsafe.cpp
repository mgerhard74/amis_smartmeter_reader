#include "Failsafe.h"
//DO NOT include project-specific headers here, as this should be as standalone as possible 
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Updater.h>


const char *FailsafeClass::failsafePage()
{
    static const char page[] PROGMEM =
R"(
<!doctype html>
<html lang="de" style="font-family:Arial;">
<head>
<title>Failsafe</title>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
<h1>Failsafe Modus</h1>
<p>Firmware Update ist möglich. Bitte die passende Datei auswählen und "Update" drücken.</p>
<form method='POST' action='/update' enctype='multipart/form-data'>
    <input type='file' name='update' required>
    <input type='submit' value='Update'>
</form>
<br>
<form method='POST' action='/reset'>
    <input type='submit' value='Reset'>
</form>
<br>
<div id="wait"></div>
<script>
document.querySelector("form[action='/update']").addEventListener("submit", function() {
    document.getElementById("wait").innerHTML='Bitte warten, das dauert jetzt etwas ...';
});
</script>
</body>
</html>
)";
    return page;
}

FailsafeClass::FailsafeClass()
{
}

bool FailsafeClass::check()
{
    
    if (!readBootState()) {
        Serial.println("Failsafe check: no valid boot state found, initializing");
        _bootState.setMagic(FAILSAFE_BOOTSTATE_MAGIC);
        _bootState.setBootCount(0);
    }
    _bootState.incrementBootCount();
    _active = (_bootState.getBootCount() >= FAILSAFE_MAX_REBOOTS);
        Serial.printf("Failsafe check: boot_count=%u active=%s magic=0x%04X\n", _bootState.getBootCount(), _active ? "true" : "false", _bootState.getMagic());

    if (_active) {
        _bootState.setBootCount(0); // reset counter so next reboot can try normal boot
        startFailsafeMode();
    } else {
        scheduleStableClear(); //not enough reboots yet for failsafe, schedule clear if the device stays up under normal operation
    }

    writeBootState();

    return _active;
}

void FailsafeClass::clearBootState()
{
    _bootState.setBootCount(0);
    writeBootState();
}

bool FailsafeClass::readBootState()
{
    if (!ESP.rtcUserMemoryRead(FAILSAFE_RTC_OFFSET, (uint32_t*)&_bootState, sizeof(_bootState))) {
        return false;
    }
    return _bootState.getMagic() == FAILSAFE_BOOTSTATE_MAGIC; //check for valid magic to detect uninitialized state (or brownout)
}

void FailsafeClass::writeBootState()
{
    Serial.printf("Failsafe writeBootState: magic=0x%04X boot_count=%u\n", _bootState.getMagic(), _bootState.getBootCount());

    uint32_t bootState = _bootState.getBootState();
    if(!ESP.rtcUserMemoryWrite(FAILSAFE_RTC_OFFSET, &bootState, sizeof(bootState))) {
        Serial.println("Failsafe writeBootState: rtcUserMemoryWrite failed!");
    }
}

void FailsafeClass::scheduleStableClear()
{
    _clearPending = true;
    _clearAtMs = millis() + (FAILSAFE_CLEAR_AFTER_SEC * 1000);
}

void FailsafeClass::scheduleRestart(uint32_t delayMs)
{
    _restartPending = true;
    _restartAtMs = millis() + delayMs;
}

void FailsafeClass::setupWifiAp()
{
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    IPAddress ip(192, 168, 4, 1);
    IPAddress mask(255, 255, 255, 0);
    WiFi.softAPConfig(ip, ip, mask);

    const char *pass = FAILSAFE_AP_PASS;
    if (strlen(pass) >= 8) {
        WiFi.softAP(FAILSAFE_AP_SSID, pass);
    } else {
        WiFi.softAP(FAILSAFE_AP_SSID);
    }

    while (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
        delay(10);
    }
}

void FailsafeClass::setupServer()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    _server.on("/", HTTP_GET, std::bind(&FailsafeClass::handleRoot, this, _1));
    _server.on("/update", HTTP_POST,
        std::bind(&FailsafeClass::handleUpdate, this, _1),
        std::bind(&FailsafeClass::handleUpdateUpload, this, _1, _2, _3, _4, _5, _6));
    _server.on("/reset", HTTP_POST, std::bind(&FailsafeClass::handleReset, this, _1));
    _server.onNotFound(std::bind(&FailsafeClass::handleNotFound, this, _1));
    _server.begin();
}

void FailsafeClass::startFailsafeMode()
{
    if (!_active) {
        return;
    }

    setupWifiAp();
    setupServer();

    pinMode(LED_BUILTIN, OUTPUT);

    _failsafeStartMs = millis();
}

bool FailsafeClass::checkAuth(AsyncWebServerRequest *request)
{
    const char *user = FAILSAFE_AUTH_USER;
    if (user[0] == '\0') {
        return true;
    }
    if (!request->authenticate(user, FAILSAFE_AUTH_PASS)) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

void FailsafeClass::handleRoot(AsyncWebServerRequest *request)
{
    if (!checkAuth(request)) {
        return;
    }
    request->send_P(200, "text/html", failsafePage());
}

void FailsafeClass::handleUpdate(AsyncWebServerRequest *request)
{
    if (!checkAuth(request)) {
        return;
    }

    if (!_uploadSeen) {
        request->send(400, "text/plain", "no file uploaded");
        return;
    }

    const bool ok = _updateOk && !Update.hasError();
    if (ok) {
        request->send(200, "text/html", "<html><body>Update OK, rebooting...</body></html>");
        scheduleRestart(500);
    } else {
        request->send(500, "text/html", "<html><body>Update failed.</body></html>");
    }
    _uploadSeen = false;
    _updateOk = true;
}

void FailsafeClass::handleUpdateUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t* data, size_t len, bool final)
{
    if (!checkAuth(request)) {
        return;
    }

    if (!index) {
        _uploadSeen = true;
        _updateOk = true;
        if (filename.isEmpty()) {
            _updateOk = false;
            return;
        }
        size_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        Update.runAsync(true);
        if (!Update.begin(maxSketchSpace)) {
            Update.printError(Serial);
            _updateOk = false;
        }
    }

    if (_updateOk && len) {
        if (Update.write(data, len) != len) {
            Update.printError(Serial);
            _updateOk = false;
        }
    }

    if (final && _updateOk) {
        if (!Update.end(true)) {
            Update.printError(Serial);
            _updateOk = false;
        }
    }
}

void FailsafeClass::handleReset(AsyncWebServerRequest *request)
{
    if (!checkAuth(request)) {
        return;
    }
    request->send(200, "text/html", "<html><body>Rebooting...</body></html>");
    scheduleRestart(200);
}

void FailsafeClass::handleNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "404 not found");
}

bool FailsafeClass::loop()
{
    if (_active) {
        if (_restartPending && (int32_t)(millis() - _restartAtMs) >= 0) {
            ESP.restart();
        }

        if ((millis() - _failsafeStartMs) >= (FAILSAFE_RETRY_BOOT_SEC * 1000)) {
            clearBootState();
            ESP.restart();
        }

        if(millis() % 300 < 150) { //blinking LED to indicate failsafe mode
            if(!_ledState) {
                digitalWrite(LED_BUILTIN, HIGH);
                _ledState = true;
            }
        } else {
            if(_ledState) {
                digitalWrite(LED_BUILTIN, LOW);
                _ledState = false;
            }
        }
        return true;
    }

    if (_clearPending && (int32_t)(millis() - _clearAtMs) >= 0) {
        clearBootState();
        _clearPending = false;
    }

    return false;
}

FailsafeClass Failsafe;

/* vim:set ts=4 et: */
