#pragma once
#include <Arduino.h>

#define ERRBLINK_MODE_OFF 0
#define ERRBLINK_MODE_SLOW 1
#define ERRBLINK_MODE_FAST 2

class StatusLights
{
private:
    uint8_t comm, err;
    uint8_t in_commblink;
    uint32_t commblink_start;
    uint8_t errblink_mode;

public:
    StatusLights(uint8_t comm, uint8_t err) : comm(comm), err(err){};
    void begin();
    void update(uint32_t ms);
    void rx_msg_blink();
    void err_blink(uint8_t errblink_mode);
};