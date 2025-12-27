// TODO(anyone) - refactor this stuff
#pragma once

//#define DEBUG
#ifndef DEBUG
    #define eprintf( fmt, args... )
    #define DBGOUT(...)
#else
    #if DEBUGHW>0
        #define FOO(...) __VA_ARGS__
        #define DBGOUT dbg_string+= FOO
        #if (DEBUGHW==2)
            #define eprintf(fmt, args...) S.printf(fmt, ##args)
        #elif (DEBUGHW==1 || DEBUGHW==3)
            #define eprintf(fmt, args...) {snprintf(dbg,sizeof(dbg),fmt, ##args);dbg_string+=dbg;dbg[0]=0;}
        #endif
    #else
        #define eprintf( fmt, args... )
        #define DBGOUT(...)
    #endif
#endif

/* vim:set ts=4 et: */
