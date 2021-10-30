#pragma once
#include <Arduino.h>

#define HEATER_PERIOD 1000 //ms. on and off time sums to a constant period

class Heater
{
private:
    static uint32_t last_period_start; //same for both instances
    static Heater* instances[2];
    static uint8_t instance_count;
    uint8_t switch_pin;
    uint8_t duty; //0-100% time on
    uint16_t on_time_ms;

public:
    Heater(uint8_t switch_pin);
    void begin();
    void off();
    static void update_all();
    void set_duty(uint8_t duty);
    uint8_t get_duty();
};