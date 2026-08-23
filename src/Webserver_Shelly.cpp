/*
    Shelly emulation: "RPC over HTTP"

    Serves the endpoints a real Shelly (Gen2+) provides, so consumers which talk to a
    Shelly the way the original device does can use the AMIS reader as their smartmeter
    (Solakon One, Hoymiles, Anker, ...).

        GET  /shelly                     -> device information (also used by Gen1 clients)
        GET  /rpc                        -> list of the supported methods
        GET  /rpc/<Method>[?id=0]        -> the "result" object of <Method>
        POST /rpc  {"id":1,"method":...} -> full json rpc response

    Announcing the device via mDNS is done in Network.cpp, the responses itself are built
    by ShellySmartmeterEmulationClass - the very same ones the UDP API uses.

    NOTE: These endpoints are intentionally not protected by the webserver credentials.
          A consumer can not authenticate itself and a real Shelly is unprotected by default too.
*/

#include "Webserver_Shelly.h"

#include "Json.h"
#include "Log.h"
#define LOGMODULE LOGMODULE_SHELLY
#include "ShellySmartmeterEmulation.h"

// Enough for the biggest supported result plus the surrounding json rpc response
#define SHELLY_RPC_RESPONSE_JSON_CAPACITY   (SHELLY_RPC_RESULT_JSON_CAPACITY + JSON_OBJECT_SIZE(4) + 64)

// Requests bigger than that are surely not a json rpc request of a smartmeter consumer
#define SHELLY_RPC_MAX_REQUEST_SIZE         512

// Size of the response buffers. The biggest response ("EM.GetStatus") is 468 bytes,
// the json rpc envelope around it adds ~80 more. Keeping these tight matters: a
// consumer polls us about once a second and the default of 1460 bytes would just
// stress the heap.
#define SHELLY_RPC_RESULT_STREAM_SIZE       512
#define SHELLY_RPC_RESPONSE_STREAM_SIZE     640
#define SHELLY_RPC_ERROR_STREAM_SIZE        128


void WebserverShellyClass::init(AsyncWebServer& server)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;

    server.on("/shelly", HTTP_GET, std::bind(&WebserverShellyClass::onShellyRequest, this, _1));

    // A handler registered on "/rpc" is called for "/rpc" and for everything below "/rpc/"
    server.on("/rpc", HTTP_GET, std::bind(&WebserverShellyClass::onRpcGetRequest, this, _1));
    server.on("/rpc", HTTP_POST,
              std::bind(&WebserverShellyClass::onRpcPostRequest, this, _1),
              nullptr,
              std::bind(&WebserverShellyClass::onRpcPostBody, this, _1, _2, _3, _4, _5));
}

/*
    GET /shelly

    Returns the device information without the json rpc envelope.
*/
void WebserverShellyClass::onShellyRequest(AsyncWebServerRequest* request)
{
    sendRpcResult(request, "Shelly.GetDeviceInfo");
}

/*
    GET /rpc/<Method>

    A real Shelly answers a HTTP GET with the plain "result" object (no json rpc envelope).
    "GET /rpc" (without a method) returns the list of the available methods.
*/
void WebserverShellyClass::onRpcGetRequest(AsyncWebServerRequest* request)
{
    const String &url = request->url();

    // strlen("/rpc/") == 5
    if (url.length() <= 5) {
        sendRpcResult(request, "Shelly.ListMethods");
        return;
    }

    sendRpcResult(request, url.c_str() + 5);
}

/*
    POST /rpc  with a json rpc request as body, eg {"id":1,"method":"EM.GetStatus","params":{"id":0}}

    The body has been collected by onRpcPostBody() into request->_tempObject.
*/
void WebserverShellyClass::onRpcPostRequest(AsyncWebServerRequest* request)
{
    if (!ShellySmartmeterEmulation.httpApiEnabled()) {
        request->send(404);
        return;
    }
    if (!request->_tempObject) {
        sendRpcError(request, 400, nullptr);
        return;
    }

    StaticJsonDocument<JSON_OBJECT_SIZE(8) + 128> requestJson;
    DeserializationError error = deserializeJson(requestJson, (const char*) request->_tempObject);
    if (error) {
        LOGF_EP("Failed to parse json. Error '%s'", error.c_str());
        sendRpcError(request, 400, nullptr);
        return;
    }
    if (!requestJson["method"].is<const char*>()) {
        sendRpcError(request, 400, nullptr);
        return;
    }
    const char *method = requestJson["method"];

    DynamicJsonDocument responseJson(SHELLY_RPC_RESPONSE_JSON_CAPACITY);
    if (!responseJson.capacity()) {
        LOG_EP("Json rpc response: Out of memory");
        request->send(500);
        return;
    }
    responseJson["id"] = requestJson["id"] | 1;
    responseJson["src"] = ShellySmartmeterEmulation.getDeviceId();
    if (requestJson["src"].is<const char*>()) {
        responseJson["dst"] = requestJson["src"];
    }
    JsonObject result = responseJson.createNestedObject("result");

    if (!ShellySmartmeterEmulation.buildRpcResult(method, result)) {
        // No valid meter data yet? Then the method is fine, we just can't answer it (yet)
        sendRpcError(request, ShellySmartmeterEmulation.hasValidData() ? 404 : 503, method);
        return;
    }

    AsyncResponseStream *response = request->beginResponseStream(F("application/json"), SHELLY_RPC_RESPONSE_STREAM_SIZE);
    SERIALIZE_JSON_LOG(responseJson, *response);
    request->send(response);
}

void WebserverShellyClass::onRpcPostBody(AsyncWebServerRequest* request, uint8_t *data, size_t len, size_t index, size_t total)
{
    if (!ShellySmartmeterEmulation.httpApiEnabled()) {
        return;
    }
    if (total == 0 || total > SHELLY_RPC_MAX_REQUEST_SIZE) {
        LOGF_EP("Invalid rpc request size %u", total);
        return;
    }

    if (index == 0) {
        // Freed by the destructor of AsyncWebServerRequest. One extra byte so the
        // buffer is always '\0' terminated for the json parser.
        request->_tempObject = calloc(total + 1, 1);
        if (!request->_tempObject) {
            LOG_EP("Out of memory receiving rpc request");
            return;
        }
    }
    if (request->_tempObject && (index + len) <= total) {
        memcpy((uint8_t*) request->_tempObject + index, data, len);
    }
}

/*
    Builds the "result" object of 'method' and sends it as response.
*/
bool WebserverShellyClass::sendRpcResult(AsyncWebServerRequest* request, const char *method)
{
    if (!ShellySmartmeterEmulation.httpApiEnabled()) {
        request->send(404);
        return false;
    }

    DynamicJsonDocument resultJson(SHELLY_RPC_RESULT_JSON_CAPACITY);
    if (!resultJson.capacity()) {
        LOG_EP("Json rpc result: Out of memory");
        request->send(500);
        return false;
    }

    JsonObject result = resultJson.to<JsonObject>();
    if (!ShellySmartmeterEmulation.buildRpcResult(method, result)) {
        // No valid meter data yet? Then the method is fine, we just can't answer it (yet)
        sendRpcError(request, ShellySmartmeterEmulation.hasValidData() ? 404 : 503, method);
        return false;
    }

    LOGF_DP("Serving '%s' to " PRsIP, method, PRIPVal(request->client()->remoteIP()));

    AsyncResponseStream *response = request->beginResponseStream(F("application/json"), SHELLY_RPC_RESULT_STREAM_SIZE);
    SERIALIZE_JSON_LOG(resultJson, *response);
    request->send(response);
    return true;
}

/*
    Error response in the format a real Shelly uses, eg
        {"code":404,"message":"No handler for EM.GetStatus"}
*/
void WebserverShellyClass::sendRpcError(AsyncWebServerRequest* request, int code, const char *method)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(2) + 96> errorJson;
    errorJson["code"] = code;
    if (code == 503) {
        LOGF_WP("No valid meter data available for method: %s", method);
        errorJson["message"] = F("No valid meter data available");
    } else if (method) {
        LOGF_WP("Unknown method: %s", method);
        errorJson["message"] = String(F("No handler for ")) + method;
    } else {
        errorJson["message"] = F("Invalid request");
    }

    AsyncResponseStream *response = request->beginResponseStream(F("application/json"), SHELLY_RPC_ERROR_STREAM_SIZE);
    response->setCode(code);
    SERIALIZE_JSON_LOG(errorJson, *response);
    request->send(response);
}

/* vim:set ts=4 et: */
