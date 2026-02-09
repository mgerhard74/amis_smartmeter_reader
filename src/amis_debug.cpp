#include "amis_debug.h"

#if DEBUG_ENABLE

    #if DEBUG_ENABLE_SW_SERIAL
    #include <SoftwareSerial.h>
    static SoftwareSerial* g_sw_serial = nullptr;
    #endif

void Debug::Init() {
    #if DEBUG_ENABLE_SW_SERIAL
        #if (DEBUG_SW_SERIAL_TX_PIN == DEBUG_UART0_TX_PIN) || \
            (DEBUG_SW_SERIAL_TX_PIN == DEBUG_UART0_RX_PIN) || \
            (DEBUG_SW_SERIAL_TX_PIN == DEBUG_UART0_TX_PIN_SWAP) || \
            (DEBUG_SW_SERIAL_TX_PIN == DEBUG_UART0_RX_PIN_SWAP) || \
            (DEBUG_SW_SERIAL_TX_PIN == DEBUG_UART1_TX_PIN)
            #error "DEBUG_ENABLE_SW_SERIAL pin conflicts with UART0/UART1 pin. Choose a different GPIO."
        #endif
        if (g_sw_serial == nullptr) {
            static SoftwareSerial sw(-1, DEBUG_SW_SERIAL_TX_PIN);
            g_sw_serial = &sw;
        }
        g_sw_serial->begin(9600); // SoftwareSerial is usually not very fast, so 9600 baud is a common choice.
    #endif
    #if DEBUG_ENABLE_SERIAL1
        #ifdef AMISREADER_SERIAL_NO==2
        #error "AMIS reader IR-LEDs are connected to Serial1. Using Serial1 for debug output will break AmisReader::loop() and likely leads to a dead-loop."
        #endif
        Serial1.begin(115200, SERIAL_8N1);
    #endif
    #if DEBUG_ENABLE_SERIAL
        #ifdef AMISREADER_SERIAL_NO==1
        #error "AMIS reader IR-LEDs are connected to Serial. Using Serial for debug output will break AmisReader::loop() and likely leads to a dead-loop."
        #endif
        Serial.begin(115200, SERIAL_8N1);
    #endif
}

void Debug::Handle() {
    #if DEBUG_ENABLE_REMOTEDEBUG
    
    // Check if we should start or stop RemoteDebug based on WiFi status
    const bool wifi_ready = ((WiFi.status() == WL_CONNECTED) || 
                            (((WiFi.getMode() & WIFI_AP) != 0) && (WiFi.softAPgetStationNum() > 0)));

    if(!_remote_debug_started) { // RemoteDebug is not started, check if WiFi is ready to start it
        if(!wifi_ready) return; // WiFi is not ready, no need to check further
        
        RemoteDebugBegin(); // Start RemoteDebug if it's not started and WiFi is ready
    } 
    
    if(_remote_debug_started) {
        if(!wifi_ready) { // WiFi is not ready anymore, stop RemoteDebug
            RemoteDebugEnd();
            return; // No need to handle RemoteDebug if it's stopped
        }

        _remote_debug.handle(); // Handle RemoteDebug if it's started and WiFi is ready
    }
    
    #endif
}

bool Debug::ShouldFlush(bool force_flush) {
    return force_flush || (DEBUG_FLUSH_ALWAYS != 0);
}

void Debug::Flush(bool force_flush) {
    if (!ShouldFlush(force_flush)) {
        return;
    }
    #if DEBUG_ENABLE_REMOTEDEBUG
    if (_remote_debug_started) {
        _remote_debug.flush();
    }
    #endif
    #if DEBUG_ENABLE_SERIAL
        Serial.flush();
    #endif
    #if DEBUG_ENABLE_SERIAL1
        Serial1.flush();
    #endif
    #if DEBUG_ENABLE_SW_SERIAL
    if (g_sw_serial) {
        g_sw_serial->flush();
    }
    #endif
}

void Debug::WriteRaw(const char *msg, bool force_flush) {
    if (!msg) {
        return;
    }
    #if DEBUG_ENABLE_REMOTEDEBUG
    if (_remote_debug_started) {
        _remote_debug.print(msg);
    }
    #endif
    #if DEBUG_ENABLE_SERIAL
    Serial.print(msg);
    #endif
    #if DEBUG_ENABLE_SERIAL1
    Serial1.print(msg);
    #endif
    #if DEBUG_ENABLE_SW_SERIAL
    if (g_sw_serial) {
        g_sw_serial->print(msg);
    }
    #endif
    
    Flush(force_flush);
}

void Debug::WriteRaw(const String &msg, bool force_flush) {
    WriteRaw(msg.c_str(), force_flush);
}

void Debug::WriteRaw(const __FlashStringHelper *msg, bool force_flush) {
    WriteRaw(String(msg).c_str(), force_flush);
}

void Debug::VPrintf(bool force_flush, const char *fmt, va_list args) {
    char buffer[DEBUG_BUFFER_SIZE];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    WriteRaw(buffer, force_flush);
}

void Debug::Printf(bool force_flush, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VPrintf(force_flush, fmt, args);
    va_end(args);
}

void Debug::WritePrefix(bool force_flush, const char *file, int line, const char *func) {
    Printf(force_flush, "[%s:%d:%s] ", file, line, func);
}

void Debug::Out(const char *msg) {
    WriteRaw(msg, false);
}

void Debug::Out(const String &msg) {
    Out(msg.c_str());
}

void Debug::Out(const __FlashStringHelper *msg) {
    Out(String(msg).c_str());
}

void Debug::OutFlush(const char *msg) {
    WriteRaw(msg, true);
}

void Debug::OutFlush(const String &msg) {
    OutFlush(msg.c_str());
}

void Debug::OutFlush(const __FlashStringHelper *msg) {
    OutFlush(String(msg).c_str());
}

#if DEBUG_ENABLE_REMOTEDEBUG
void Debug::RemoteDebugBegin() {
    _remote_debug.begin("AmisDebug");
    _remote_debug.setSerialEnabled(false); // serial outputs handled locally
    _remote_debug.setResetCmdEnabled(true);
    _remote_debug_started = true;
    Out("RemoteDebug started\n");
}

void Debug::RemoteDebugEnd() {
    _remote_debug.stop();
    _remote_debug_started = false;
    Out("RemoteDebug stopped\n");
}

RemoteDebug Debug::_remote_debug;
bool Debug::_remote_debug_started = false;
#endif
#endif
