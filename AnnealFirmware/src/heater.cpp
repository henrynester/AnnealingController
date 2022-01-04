#include "heater.h"

extern const uint16_t LOOP_PERIOD;

void Heater::begin()
{
    //switch pin is an output. heater off to start.
    pinMode(switch_pin, OUTPUT);
    digitalWrite(switch_pin, LOW);

    pinMode(sense_pin, INPUT_PULLUP);
}

void Heater::update(uint16_t ms)
{
    //heater on from period start->period start+period length*duty cycle
    uint8_t state = 0;
    if (duty == 0)
    {
        state = 0;
    }
    else if (duty == 100)
    {
        state = 1;
    }
    else
    {
        state = (ms < on_time_ms) ? 1 : 0;
    }
    digitalWrite(switch_pin, state);
    powered = digitalRead(sense_pin);
}

void Heater::shutdown()
{
    set_duty(0);
    digitalWrite(switch_pin, LOW);
}

void Heater::set_duty(uint8_t duty)
{
    this->duty = constrain(duty, 0, 100);
    on_time_ms = (uint16_t)((float)LOOP_PERIOD / 100.0 * duty);
}

uint8_t Heater::get_duty()
{
    return duty;
}

bool Heater::has_power()
{
    return powered;
}