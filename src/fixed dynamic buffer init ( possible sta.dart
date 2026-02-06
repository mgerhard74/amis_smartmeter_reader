
/*
    request looks like: 
        {"id":1,"method":"EM1.GetStatus","params":{"id":0}}
    response looks like:    
        {"id":1,"src":"shellyproem50-someid","dst":"unknown","result":{"act_power":100.0}}
        {"id":1,"src":"shellypro3em-someid","dst":"unknown","result":{"a_act_power":100.0, "b_act_power":100.0,"c_act_power":100.0,"total_act_power":300.0}}
*/
void ShellySmartmeterEmulationClass::handleRequest(AsyncUDPPacket udpPacket) {
    DBG("got packet");

    if(!_currentValues.dataAreValid) {
        return;
    }

    // --- JSON parsen ---
    size_t len = udpPacket.length();
    const size_t MAX_PACKET_SIZE = 128;  // limit valid packet size, prevents stack overflow receiving malformed packages
    if(len == 0 || len > MAX_PACKET_SIZE) {
        DBG("[ DEBUG ] Invalid packet size\n");
        return;
    }
    char tempBuffer[MAX_PACKET_SIZE + 1];
    memcpy(tempBuffer, udpPacket.data(), len);
    tempBuffer[len] = '\0';
    DBG("received data: %s\n", tempBuffer);

    StaticJsonBuffer<MAX_PACKET_SIZE*2> jsonBufferRequest; //need more space for parsing
    JsonObject& requestJson = jsonBufferRequest.parseObject(tempBuffer);
    if (!requestJson.success()) {
        DBG("[ DEBUG ] Failed to parse json\n");
        return;
    }

    //check for objects
    if( !requestJson.containsKey("id") || !requestJson["id"].is<int>() || 
        !requestJson.containsKey("method") || !requestJson["method"].is<char*>() ||
        !requestJson.containsKey("params")) 
    {
        DBG("[ DEBUG ] Invalid json\n");
        return;
    }
    JsonObject& params = requestJson["params"];
    if (!params.containsKey("id") || !params["id"].is<int>()) {
        DBG("[ DEBUG ] Invalid json\n");
        return;
    }
    
    int id = requestJson["id"];
    String method(requestJson["method"]);

    StaticJsonBuffer<256> jsonBufferResponse;
    JsonObject& responseJson = jsonBufferResponse.createObject();
    responseJson["id"] = id;
    responseJson["src"] = _device.id;
    responseJson["dst"] = "unknown";
    responseJson["result"] =  jsonBufferResponse.createObject();
    //the B2500 is VEEEERY picky... needs "float" formatted value with a dot
    String saldo = String(_currentValues.saldo + _offset)+".0"; 
    if(method =="EM.GetStatus") {
        responseJson["result"]["a_act_power"] = RawJson(saldo);
        responseJson["result"]["b_act_power"] = RawJson("0.0");
        responseJson["result"]["c_act_power"] = RawJson("0.0");
        responseJson["result"]["total_act_power"] = RawJson(saldo);
    } else if(method == "EM1.GetStatus") {
        responseJson["result"]["act_power"] = RawJson(saldo);
    } else {
        DBG("[ DEBUG ] unknown method\n");
        return;
    }

    AsyncUDPMessage message;
    responseJson.printTo(message);
    _udp.sendTo(message, udpPacket.remoteIP(), udpPacket.remotePort()); //respond directly via "udpPacket" doesn't work
    DBG("sent response: %s",message.data());
}
