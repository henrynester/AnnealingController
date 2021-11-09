#include <heater.h>

uint32_t Heater::last_period_start = 0;
uint8_t Heater::instance_count = 0;
Heater *Heater::instances[2] = {nullptr, nullptr};

// Heater::Heater(uint8_t switch_pin, uint8_t sense_pin)
// {

//     //initialize locals
//     this->switch_pin = switch_pin;
//     this->sense_pin = sense_pin;
// }

void Heater::begin()
{
    //switch pin is an output. heater off to start.
    pinMode(switch_pin, OUTPUT);
    digitalWrite(switch_pin, LOW);
}

void Heater::update(uint32_t ms)
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
        state = (ms - last_period_start < on_time_ms) ? 1 : 0;
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
    on_time_ms = (uint16_t)((float)HEATER_PERIOD / 100.0 * duty);
}

uint8_t Heater::get_duty()
{
    return duty;
}

bool Heater::has_power()
{
    return powered;
}

void Heater::update_all(uint32_t ms)
{
    for (uint8_t i = 0; i < instance_count; i++)
    {
        instances[i]->update(ms);
    }
    if (ms - last_period_start > HEATER_PERIOD)
    {
        last_period_start = ms;
    }
}

void Heater::shutdown_all()
{
    for (uint8_t i = 0; i < instance_count; i++)
    {
        instances[i]->shutdown();
    }
}