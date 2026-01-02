#pragma once

// Die Werte der Klasse Application sollten nur einmalig am Beginn gesetzt werden.
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

extern ApplicationClass Application;

/* vim:set ts=4 et: */
