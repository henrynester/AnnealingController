#pragma once
#include <Arduino.h>

#define HEATER_PERIOD 1000 //ms. on and off time sums to a constant period

class Heater
{
private:
    static uint32_t last_period_start; //same for both instances
    static Heater *instances[2];
    static uint8_t instance_count;
    uint8_t switch_pin, sense_pin;
    uint8_t duty; //0-100% time on
    uint16_t on_time_ms;
    bool powered;
    void update(uint32_t ms);

public:
    Heater(uint8_t switch_pin, uint8_t sense_pin) : switch_pin(switch_pin), sense_pin(sense_pin)
    {
        //add a pointer to the newly-constructed Heater object to the instance list
        instances[instance_count] = this;
        instance_count++;
    };
    void begin();
    void shutdown();
    void set_duty(uint8_t duty);
    uint8_t get_duty();
    bool has_power();

    static void update_all(uint32_t ms);
    static void shutdown_all();
};