#pragma once

class RebootClass {
public:
    void init(void);
    void startReboot();
    bool startUpdateFirmware();
    void endUpdateFirmware();
    void softreset();
    void loop();

private:
    int _state = 0;
};
extern RebootClass Reboot;

/* vim:set ts=4 et: */
