#include "Webserver.h"

#include <LittleFS.h>

#include "debug.h"

WebserverClass::WebserverClass()
    : _server(WEBSERVER_HTTP_PORT)
{
}

void WebserverClass::init(bool upgradeMode)
{
    using std::placeholders::_1;

    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");   //CORS-Header allgem.

    // Initilisieren der einzelnen Handler für diverse Unterverzeichnisse
    _websrvLogin.init(_server);
    _websrvRest.init(_server);
    _websrvUpdate.init(_server);

    // Initilisieren der Websocket Handler
    _websrvWsConsole.init(_server);
    _websrvWsData.init(_server);

    _server.onNotFound(std::bind(&WebserverClass::onNotFound, this, _1));

    // Spezielle statische Seite "/upgrade"
    _server.on("/upgrade", HTTP_GET, std::bind(&WebserverClass::onRequest_Upgrade, this, _1));

    // Der Rest kommt aus dem Filesystem
    _server.serveStatic("/", LittleFS, "/", "public, must-revalidate");  // /*.* wird aut. geservt, alle Files die keine Daten anfordern (GET, POST...)

    if (upgradeMode) {
        _server.rewrite("/", "/upgrade");
    } else {
        _server.rewrite("/", "/index.html");
    }

    _server.begin();
}

void WebserverClass::onNotFound(AsyncWebServerRequest *request)
{
    if (request->method() == HTTP_OPTIONS) {
        DBGOUT(F("HTTP-Options\n"));
        request->send(200);
    } else {
        request->send(404, F("text/plain"), F("404 not found!"));
    }
}

void WebserverClass::reload()
{
    _websrvWsConsole.reload();
    _websrvWsData.reload();
}


// TODO: Brauchen wir das wirklich noch?
void WebserverClass::onRequest_Upgrade(AsyncWebServerRequest *request)
{
    static const char _page_upgrade[] PROGMEM = R\
"=====(
<!doctype html>
<html lang="de" style="font-family:Arial;">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1" />
<h1>Version Upgrade</h1><br>
Upgrade vervollständigen: Im Filedialog bitte 'firmware.bin', 'littlefs.bin' oder eine andere Datei auswählen.<br><br><br>
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

    request->send(200, F("text/html"), _page_upgrade);
}

WebserverClass Webserver;

/* vim:set ts=4 et: */
