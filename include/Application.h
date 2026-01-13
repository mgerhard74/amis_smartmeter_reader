#pragma once

// Die Werte der Klasse ApplicationClass sollten nur einmalig am Beginn gesetzt werden.
// Ansonsten gehören sie in die Config

typedef struct {
    bool inAPMode;
} Application_t;

class ApplicationClass
{
    public:
        inline void init(bool inAPMode) {_appData.inAPMode = inAPMode; };
        inline bool inAPMode() { return _appData.inAPMode; };
    private:
        Application_t _appData;

};


// Die Werte der Klasse ApplicationRuntimeClass werden nur während der Laufzeit durch externe Aufrufe geändert
// Werden nicht gespeichert und sind nur bis zum nächste Reboot gültig
typedef struct {
    bool webUseFilesFromFirmware = true;
} ApplicationRuntime_t;

class ApplicationRuntimeClass
{
    public:
        inline bool webUseFilesFromFirmware() { return _runtimeData.webUseFilesFromFirmware; };
        inline void webUseFilesFromFirmware(bool webUseFilesFromFirmware) { _runtimeData.webUseFilesFromFirmware = webUseFilesFromFirmware; };
    private:
        ApplicationRuntime_t _runtimeData;

};


extern ApplicationClass Application;
extern ApplicationRuntimeClass ApplicationRuntime;

/* vim:set ts=4 et: */
