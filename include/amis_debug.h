#pragma once

#include "proj.h"

#include <stdarg.h>
#include <stdio.h>

#ifndef DEBUG_ENABLE
    #define DEBUG_ENABLE 0
#endif
#ifndef DEBUG_ENABLE_SERIAL
    #define DEBUG_ENABLE_SERIAL 0
#endif
#ifndef DEBUG_ENABLE_SERIAL1
    #define DEBUG_ENABLE_SERIAL1 0
#endif
#ifndef DEBUG_ENABLE_REMOTEDEBUG
    #define DEBUG_ENABLE_REMOTEDEBUG 1 //RemoteDebug is enabled by default (if DEBUG_ENABLE is enabled)
#endif
#ifndef DEBUG_ENABLE_WITH_CONTEXT
    #define DEBUG_ENABLE_WITH_CONTEXT 0
#endif
#ifndef DEBUG_FLUSH_ALWAYS
    #define DEBUG_FLUSH_ALWAYS 0
#endif
#ifndef DEBUG_BUFFER_SIZE
    #define DEBUG_BUFFER_SIZE 256
#endif
#ifndef DEBUG_ENABLE_SW_SERIAL
    // Set to a GPIO number to enable SoftwareSerial debug TX on that pin.
    // Default: GPIO14 (the SoftAP Pin), avoids UART0/UART1 pins.
    #define DEBUG_ENABLE_SW_SERIAL 0
#endif
#ifndef DEBUG_SW_SERIAL_TX_PIN
    #define DEBUG_SW_SERIAL_TX_PIN 14 // Default to GPIO14 (the SoftAP Pin)
#endif
#ifndef DEBUG_UART0_TX_PIN
    #if defined(ARDUINO_ARCH_ESP8266)
        #define DEBUG_UART0_TX_PIN 1
        #define DEBUG_UART0_RX_PIN 3
        #define DEBUG_UART0_TX_PIN_SWAP 15
        #define DEBUG_UART0_RX_PIN_SWAP 13
        #define DEBUG_UART1_TX_PIN 2
    #else
        // Unknown MCU: set to -1 to disable pin conflict checks by default.
        #define DEBUG_UART0_TX_PIN -1
        #define DEBUG_UART0_RX_PIN -1
        #define DEBUG_UART0_TX_PIN_SWAP -1
        #define DEBUG_UART0_RX_PIN_SWAP -1
        #define DEBUG_UART1_TX_PIN -1
    #endif
#endif

#if DEBUG_ENABLE 
    #if DEBUG_ENABLE_REMOTEDEBUG
        #include <RemoteDebug.h>
    #endif
#endif

class DebugNoOp {
public:
    static void Init() {}
    static void Handle() {}
    static bool ShouldFlush(bool force_flush) {
        (void)force_flush;
        return false;
    }
    static void Flush(bool force_flush) {
        (void)force_flush;
    }
    static void WriteRaw(const char *msg, bool force_flush) {
        (void)msg;
        (void)force_flush;
    }
    static void WriteRaw(const String &msg, bool force_flush) {
        (void)msg;
        (void)force_flush;
    }
    static void WriteRaw(const __FlashStringHelper *msg, bool force_flush) {
        (void)msg;
        (void)force_flush;
    }
    static void VPrintf(bool force_flush, const char *fmt, va_list args) {
        (void)force_flush;
        (void)fmt;
        (void)args;
    }
    static void Printf(bool force_flush, const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
        (void)force_flush;
        (void)fmt;
    }
    static void WritePrefix(bool force_flush, const char *file, int line, const char *func) {
        (void)force_flush;
        (void)file;
        (void)line;
        (void)func;
    }
    static void Out(const char *msg) {
        (void)msg;
    }
    static void Out(const String &msg) {
        (void)msg;
    }
    static void Out(const __FlashStringHelper *msg) {
        (void)msg;
    }

    template <typename... Args>
    static void Out(const char *fmt, Args... args) {
        (void)fmt;
        (void)sizeof...(args);
    }

    template <typename... Args>
    static void OutSource(const char *file, int line, const char *func, Args... args) {
        (void)file;
        (void)line;
        (void)func;
        (void)sizeof...(args);
    }

    static void OutFlush(const char *msg) {
        (void)msg;
    }
    static void OutFlush(const String &msg) {
        (void)msg;
    }
    static void OutFlush(const __FlashStringHelper *msg) {
        (void)msg;
    }

    static void OutLine(const char *msg) {
        (void)msg;
    }
    static void OutLine(const String &msg) {
        (void)msg;
    }
    static void OutLine(const __FlashStringHelper *msg) {
        (void)msg;
    }

    template <typename... Args>
    static void OutLine(const char *fmt, Args... args) {
        (void)fmt;
        (void)sizeof...(args);
    }

    template <typename... Args>
    static void OutSourceLine(const char *file, int line, const char *func, Args... args) {
        (void)file;
        (void)line;
        (void)func;
        (void)sizeof...(args);
    }

    static void OutFlushLine(const char *msg) {
        (void)msg;
    }
    static void OutFlushLine(const String &msg) {
        (void)msg;
    }
    static void OutFlushLine(const __FlashStringHelper *msg) {
        (void)msg;
    }

    template <typename... Args>
    static void OutFlushLine(const char *fmt, Args... args) {
        (void)fmt;
        (void)sizeof...(args);
    }

    template <typename... Args>
    static void OutSourceFlushLine(const char *file, int line, const char *func, Args... args) {
        (void)file;
        (void)line;
        (void)func;
        (void)sizeof...(args);
    }

    template <typename... Args>
    static void OutFlush(const char *fmt, Args... args) {
        (void)fmt;
        (void)sizeof...(args);
    }

    template <typename... Args>
    static void OutSourceFlush(const char *file, int line, const char *func, Args... args) {
        (void)file;
        (void)line;
        (void)func;
        (void)sizeof...(args);
    }
};

#if DEBUG_ENABLE
class Debug : public DebugNoOp {
public:
    static void Init();
    static void Handle();
    static bool ShouldFlush(bool force_flush);
    static void Flush(bool force_flush);
    static void WriteRaw(const char *msg, bool force_flush);
    static void WriteRaw(const String &msg, bool force_flush);
    static void WriteRaw(const __FlashStringHelper *msg, bool force_flush);
    static void VPrintf(bool force_flush, const char *fmt, va_list args);
    static void Printf(bool force_flush, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    static void WritePrefix(bool force_flush, const char *file, int line, const char *func);
    static void Out(const char *msg);
    static void Out(const String &msg);
    static void Out(const __FlashStringHelper *msg);

    template <typename... Args>
    static void Out(const char *fmt, Args... args) {
        Printf(false, fmt, args...);
    }

    static void OutLine(const char *msg) {
        WriteRaw(msg, false);
        WriteRaw("\n", false);
    }
    static void OutLine(const String &msg) {
        OutLine(msg.c_str());
    }
    static void OutLine(const __FlashStringHelper *msg) {
        OutLine(String(msg).c_str());
    }

    template <typename... Args>
    static void OutLine(const char *fmt, Args... args) {
        Printf(false, fmt, args...);
        Printf(false, "\n");
    }

    template <typename... Args>
    static void OutSource(const char *file, int line, const char *func, Args... args) {
        WritePrefix(false, file, line, func);
        Out(args...);
    }

    template <typename... Args>
    static void OutSourceLine(const char *file, int line, const char *func, Args... args) {
        WritePrefix(false, file, line, func);
        OutLine(args...);
    }

    static void OutFlush(const char *msg);
    static void OutFlush(const String &msg);
    static void OutFlush(const __FlashStringHelper *msg);

    template <typename... Args>
    static void OutFlush(const char *fmt, Args... args) {
        Printf(true, fmt, args...);
    }

    static void OutFlushLine(const char *msg) {
        WriteRaw(msg, true);
        WriteRaw("\n", true);
    }
    static void OutFlushLine(const String &msg) {
        OutFlushLine(msg.c_str());
    }
    static void OutFlushLine(const __FlashStringHelper *msg) {
        OutFlushLine(String(msg).c_str());
    }

    template <typename... Args>
    static void OutFlushLine(const char *fmt, Args... args) {
        Printf(true, fmt, args...);
        Printf(true, "\n");
    }

    template <typename... Args>
    static void OutSourceFlush(const char *file, int line, const char *func, Args... args) {
        WritePrefix(true, file, line, func);
        OutFlush(args...);
    }

    template <typename... Args>
    static void OutSourceFlushLine(const char *file, int line, const char *func, Args... args) {
        WritePrefix(true, file, line, func);
        OutFlushLine(args...);
    }

private:
#if DEBUG_ENABLE_REMOTEDEBUG
    static void RemoteDebugBegin();
    static void RemoteDebugEnd();
    static RemoteDebug _remote_debug;
    static bool _remote_debug_started;
#endif
};
#else
class Debug : public DebugNoOp {};
#endif

#if DEBUG_ENABLE
    #if DEBUG_ENABLE_WITH_CONTEXT
        #define DBG(...) Debug::OutSourceLine(__FILE__, __LINE__, __func__, __VA_ARGS__)
        #define DBG_FLUSH(...) Debug::OutSourceFlushLine(__FILE__, __LINE__, __func__, __VA_ARGS__)
        #define DBG_NOCTX(...) Debug::OutLine(__VA_ARGS__)
        #define DBG_FLUSH_NOCTX(...) Debug::OutFlushLine(__VA_ARGS__)
    #else
        #define DBG(...) Debug::OutLine(__VA_ARGS__)
        #define DBG_FLUSH(...) Debug::OutFlushLine(__VA_ARGS__)
        #define DBG_NOCTX(...) Debug::OutLine(__VA_ARGS__)
        #define DBG_FLUSH_NOCTX(...) Debug::OutFlushLine(__VA_ARGS__)
    #endif
#else
    #define DBG(...) do { } while (0)
    #define DBG_FLUSH(...) do { } while (0)
    #define DBG_NOCTX(...) do { } while (0)
    #define DBG_FLUSH_NOCTX(...) do { } while (0)
#endif

/* vim:set ts=4 et: */
