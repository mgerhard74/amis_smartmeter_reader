#pragma once

class RebootClass {
public:
    void init(void);
    void startReboot();
    bool startUpdateFirmware();
    void endUpdateFirmware();
    bool startUpdateLittleFS();
    void endUpdateLittleFS();
    void loop();

private:
    int _state = 0;
};
extern RebootClass Reboot;

/* vim:set ts=4 et: */
