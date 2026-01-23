#pragma once

#include <stdint.h>
#include <user_interface.h>

extern "C" {


typedef enum : uint8_t {
    exception_type_assert = 1,
    exception_type_panic = 2,
    exception_type_unhandled_exception = 3,
    exception_type_abort = 4
} exception_extra_info_type_t;

typedef enum : uint8_t {
    exception_context_bearssl = 1,
    exception_context_cont = 2,
    exception_context_sys = 3,
} exception_context_t;


// using numbers different from "REASON_" in user_interface.h (=0..6)
// moved from core_es8266_postmortem.cpp
enum rst_reason_sw
{
    REASON_USER_STACK_SMASH = 253,
    REASON_USER_SWEXCEPTION_RST = 254
};

typedef struct {
    uint32_t sp_dump;
    struct rst_info rst_info;
    uint32_t excsave1;
    exception_extra_info_type_t exception_extra_info_type;
    exception_context_t exception_context;
    union {
        struct {
            int line;
            const char* file = 0;
            const char* func;
            const char* what;
        } panic;
        struct {
            const char *unhandled_exception;
        } unhandled_exception;
        /*
        // Done via: exception_extra_info_type == exception_type_abort
        struct {
            bool called;
        } abort;
         */
        struct {
            uint32_t addr;
        } stacksmash;
    } u_exception_extra_info;
} postmortem_rst_info_t;

typedef void (*postmortem_report_t)(const postmortem_rst_info_t *postmortem_info, bool *doSerialReport);

extern postmortem_report_t postmortem_report_early_callback;

} // extern "C"


/* vim:set ts=4 et: */
