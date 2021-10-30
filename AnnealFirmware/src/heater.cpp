#include <heater.h>

uint32_t Heater::last_period_start = 0;
uint8_t Heater::instance_count = 0;
Heater *Heater::instances[2] = {nullptr, nullptr};

Heater::Heater(uint8_t switch_pin)
{
    this->switch_pin = switch_pin;
    instances[instance_count] = this;
    instance_count++;
}

void Heater::begin()
{
    //output, off for pin
    pinMode(switch_pin, OUTPUT);
    digitalWrite(switch_pin, LOW);
}

void Heater::off()
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

void Heater::update_all()
{
    for (uint8_t i = 0; i < instance_count; i++)
    {
        //heater on from period start->period start+period length*duty cycle
        uint8_t state = 0;
        if (instances[i]->duty == 0)
        {
            state = 0;
        }
        else if (instances[i]->duty == 100)
        {
            state = 1;
        }
        else
        {
            state = (millis() - last_period_start < instances[i]->on_time_ms) ? 1 : 0;
        }
        digitalWrite(instances[i]->switch_pin, state);
    }
    if (millis() - last_period_start > HEATER_PERIOD)
    {
        last_period_start = millis();
    }
}