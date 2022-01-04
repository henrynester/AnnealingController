#pragma once
#include <Arduino.h>

class Heater
{
private:
    uint8_t switch_pin, sense_pin;
    uint8_t duty; //0-100% time on
    uint16_t on_time_ms;
    bool powered;

public:
    Heater(uint8_t switch_pin, uint8_t sense_pin) : switch_pin(switch_pin), sense_pin(sense_pin){};
    void begin();
    void update(uint16_t ms);
    void shutdown();
    void set_duty(uint8_t duty);
    uint8_t get_duty();
    bool has_power();
};