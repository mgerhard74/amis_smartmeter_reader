#pragma once

class RebootClass {
public:
    void init(void);
    void startReboot();
    void startUpdateFirmware();
    void endUpdateFirmware();
    void startUpdateLittleFS();
    void endUpdateLittleFS();
    void loop();

private:
    int _state = 0;
};
extern RebootClass Reboot;

/* vim:set ts=4 et: */
