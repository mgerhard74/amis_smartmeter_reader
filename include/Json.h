#pragma once

#include <ArduinoJson.h>

/* Helper define to get an error log if we're out of memory */

#define SERIALIZE_JSON(__JSON, __OUTPUT) \
         serializeJson(__JSON, __OUTPUT)

#define SERIALIZE_JSON_LOG(__JSON, __OUTPUT) \
    do { \
        if (__JSON.overflowed()) { \
            LOGF_EP("Json memory error: %S:%d (%S())", __FILE__, __LINE__, __FUNCTION__); \
        } else { \
            if (__JSON.capacity() - __JSON.memoryUsage() < 32) { \
                LOGF_WP("Json low memory: %S:%d (%S()). Only %u Bytes left", __FILE__, __LINE__, __FUNCTION__, __JSON.capacity() - __JSON.memoryUsage()); \
            } else { \
                /* LOGF_IP("Json memory: %u of %u left: %S %d (%S)", __JSON.capacity() - __JSON.memoryUsage(), __JSON.capacity(), __FILE__, __LINE__, __FUNCTION__); */ \
            } \
        } \
        SERIALIZE_JSON(__JSON, __OUTPUT); \
    } while(0)


#if 0
/*
    DeserializationError error = DeserializationError::EmptyInput;
    if (xyz) {
        error = deserializeJson(json, xyz);
    }
*/
#define DESERIALIZE_JSON(__JSON, __INPUT) \
         deserializeJson(__JSON, __INTPUT)

#define DESERIALIZE_JSON_LOG(__JSON, __INTPUT, __ERROR) \
    ....

#endif

/* vim:set ts=4 et: */
