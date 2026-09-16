#include "Webserver.h"

#include "Application.h"
#include "config.h"
#include "Log.h"
#define LOGMODULE LOGMODULE_WEBSERVER
#include "Utils.h"
#include "__compiled_constants.h"

#include <LittleFS.h>

#include "__embed_data_amis_css.h"
#include "__embed_data_chart_js.h"
#include "__embed_data_cust_js.h"
#include "__embed_data_index_html.h"
#include "__embed_data_jquery371slim_js.h"
#include "__embed_data_pure_min_css.h"


// Fixe upgrade seite
static const char _page_upgrade[] PROGMEM =
R"(<!doctype html>
<html lang="de" style="font-family:Arial;">
<head>
<title>Upload file</title><!-- -->
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
<h1>Versions Upgrade / Datei Upload</h1><br>
Upgrade vervollständigen: Im Filedialog bitte 'firmware.bin' oder eine andere Datei auswählen.<br><br><br>
<form method='POST' action='/update' enctype='multipart/form-data' id="up">
    <input type='file' name='update'><input type='button' value='Update' onclick="btclick();">
</form>
<br>
<div id="wait"></div>
<script>
function btclick() {
    document.getElementById("up").submit();
    document.getElementById("wait").innerHTML='Bitte warten, das dauert jetzt etwas ...';
}
</script>
</body>
</html>
)";


WebserverClass::WebserverClass()
    : _server(WEBSERVER_HTTP_PORT)
{
    _staticFilesServer = nullptr;
}


void WebserverClass::responseBinaryDataWithETagCache(AsyncWebServerRequest* request, const char *contentType, bool utf8, const char *contentEncoding, const uint8_t* content, size_t len, const char *md5sum, bool checkCredentials)
{
    if (checkCredentials && !Webserver.checkCredentials(request, true)) {
        // Some static pages may be shipped without checking credentials
        return;
    }

    if (!ApplicationRuntime.webUseFilesFromFirmware() && _staticFilesServer) {
        if (_staticFilesServer->canHandle(request)) {
            if (request->_tempObject) {
                LOGF_DP("Serving '%s %s' from filesystem", request->methodToString(), (const char*) request->_tempObject);
            }
            _staticFilesServer->handleRequest(request);
            return;
        }
    }

    LOGF_DP("Serving '%s %s' directly from firmware", request->methodToString(), request->url().c_str());

    auto md5 = MD5Builder();
    md5.begin();
    md5.add(md5sum);

    // ensure ETag uniqueness per compiled version by including compile time.
    // This should force browsers to reload dependent resources like amis.css and cust.js
    // even when index.html content hasn't actually changed between versions.
    md5.add(__COMPILED_DATE_TIME_UTC_STR__);
    md5.calculate();

    String expectedEtag;
    expectedEtag = "\"";
    expectedEtag += md5.toString();
    expectedEtag += "\"";

    bool eTagMatch = false;
    if (request->hasHeader(asyncsrv::T_INM)) {
        const AsyncWebHeader* h = request->getHeader(asyncsrv::T_INM);
        eTagMatch = h->value().equals(expectedEtag);
    }

    // begin response 200 or 304
    AsyncWebServerResponse* response;
    if (eTagMatch) {
        response = request->beginResponse(304); // not modified (use cache)
    } else {
        String ct = contentType;
        if (utf8) {
            ct += "; charset=utf-8";
        }
        response = request->beginResponse_P(200, ct, content, len);
        //response = request->beginResponse(200, ct, content, len);
        //response = request->beginChunkedResponse(200, ct, content, len);
        if (contentEncoding && contentEncoding[0]) {
            response->addHeader(asyncsrv::T_Content_Encoding, contentEncoding);
        }
    }

    // HTTP requires cache headers in 200 and 304 to be identical
    response->addHeader(asyncsrv::T_Cache_Control, "public, must-revalidate");
    response->addHeader(asyncsrv::T_ETag, expectedEtag);
    response->addHeader(asyncsrv::T_Content_Disposition, "inline");
    // response->addHeader("last-modified", "Fri, 12 Dec 2025 13:05:45 GMT");
    response->addHeader(F("X-AMIS-From-Firmware"), "yes"); // Just for us: Indicate that we shipped it directly
    request->send(response);
}


void WebserverClass::init()
{
    using std::placeholders::_1;

    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");   //CORS-Header allgem.

    // Initilisieren der einzelnen Handler für diverse Unterverzeichnisse
    _websrvLogin.init(_server);
    _websrvRest.init(_server);
    _websrvShelly.init(_server);
    _websrvUpdate.init(_server);

    // Initilisieren der Websocket Handler
    _websrvWsData.init(_server);

    // Eine 404 (Not Found) Site
    _server.onNotFound(std::bind(&WebserverClass::onNotFound, this, _1));

    // Spezielle statische Seite "/upgrade" einbinden (Seite kommt aus der Firmware)
    _server.on("/upgrade", HTTP_GET, [&](AsyncWebServerRequest* request) {
        request->send_P(200, F("text/html; charset=utf-8"), _page_upgrade);
    });

    // Restliche Standard Seiten - ebenfalls aus der Firmware - aber mit ETag-Header
    _server.on("/amis.css", HTTP_GET, [&](AsyncWebServerRequest* request) {
        responseBinaryDataWithETagCache(request, asyncsrv::T_text_css, true,
                                        asyncsrv::T_gzip, amis_css_gz, amis_css_gz_size, amis_css_gz_md5,
                                        false);
    });
    _server.on("/chart.js", HTTP_GET, [&](AsyncWebServerRequest* request) {
        responseBinaryDataWithETagCache(request, asyncsrv::T_application_javascript, true,
                                        asyncsrv::T_gzip, chart_js_gz, chart_js_gz_size, chart_js_gz_md5,
                                        false);
    });
    _server.on("/cust.js", HTTP_GET, [&](AsyncWebServerRequest* request) {
        responseBinaryDataWithETagCache(request, asyncsrv::T_application_javascript, true,
                                        asyncsrv::T_gzip, cust_js_gz, cust_js_gz_size, cust_js_gz_md5,
                                        false);
    });
    _server.on("/index.html", HTTP_GET, [&](AsyncWebServerRequest* request) {
        responseBinaryDataWithETagCache(request, asyncsrv::T_text_html, true,
                                        asyncsrv::T_gzip, index_html_gz, index_html_gz_size, index_html_gz_md5,
                                        false);
    });
    _server.on("/jquery371slim.js", HTTP_GET, [&](AsyncWebServerRequest* request) {
        responseBinaryDataWithETagCache(request, asyncsrv::T_application_javascript, true,
                                        asyncsrv::T_gzip, jquery371slim_js_gz, jquery371slim_js_gz_size, jquery371slim_js_gz_md5,
                                        false);
    });
    _server.on("/pure-min.css", HTTP_GET, [&](AsyncWebServerRequest* request) {
        responseBinaryDataWithETagCache(request, asyncsrv::T_text_css, true,
                                        asyncsrv::T_gzip, pure_min_css_gz, pure_min_css_gz_size, pure_min_css_gz_md5,
                                        false);
    });

    // Die "Defaultseite" / behandeln
    _server.rewrite("/", "/index.html");

    // Der Rest kommt aus dem Filesystem
    _staticFilesServer = &_server.serveStatic("/", LittleFS, "/", "public, must-revalidate");  // /*.* wird aut. geservt, alle Files die keine Daten anfordern (GET, POST...)
    //_staticFilesServer->setTryGzipFirst(true); // ist default bereits enabled

    _server.begin();
}

bool WebserverClass::checkCredentials(AsyncWebServerRequest* request, bool sendErrorResponse)
{
    if (!_auth_enabled) {
        return true;
    }

    if (request->authenticate(_auth_username.c_str(), _auth_password.c_str())) {
        return true;
    }

    if (sendErrorResponse) {
        if (request->hasHeader("X-Requested-With")) {
            // If we have a header X-Requested-With prevent browser internal auth dialogs - just return 401
            AsyncWebServerResponse* r = request->beginResponse(401);
            request->send(r);
        } else {
            request->requestAuthentication();
        }
    }

    return false;
}

void WebserverClass::onNotFound(AsyncWebServerRequest *request)
{
    if (request->method() == HTTP_OPTIONS) {
        LOG_DP("Method HTTP_OPTIONS called");
        request->send(200);
    } else {
        request->send(404, F("text/plain"), F("404 not found!"));
    }
}

void WebserverClass::reloadCredentials()
{
    if (_staticFilesServer == nullptr) {
        // Webserver.init() must be called first
        return;
    }

    bool changed = false;
    if (_auth_enabled != Config.use_auth) {
        changed = true;
        _auth_enabled = Config.use_auth;
    }
    if (!_auth_username.equals(Config.auth_user)) {
        changed = true;
        _auth_username = Config.auth_user;
    }
    if (!_auth_password.equals(Config.auth_passwd)) {
        changed = true;
        _auth_password = Config.auth_passwd;
    }
    if (changed) {
        if (!_auth_enabled) {
            _staticFilesServer->setAuthentication("", "", AUTH_NONE);
        } else {
            _staticFilesServer->setAuthentication(_auth_username, _auth_password); // AUTH_DIGEST is default (AUTH_BASIC)
        }

        // Update the websockets also
        _websrvWsData.reload();
    }
}

WebserverClass Webserver;

/* vim:set ts=4 et: */
