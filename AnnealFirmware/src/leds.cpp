#include "leds.h"
#include "pins.h"

void StatusLights::begin()
{
    //set LED pins as outputs
    pinMode(comm, OUTPUT);
    pinMode(err, OUTPUT);
    //sexy startup blink to show the MCU works
    for (uint8_t i = 0; i < 10; i++)
    {
        digitalWrite(err, LOW);
        digitalWrite(comm, HIGH);
        delay(50);
        digitalWrite(comm, LOW);
        digitalWrite(err, HIGH);
        delay(50);
    }
}
void StatusLights::update(uint32_t ms)
{
    //brief flash of COMM LED on each message received over the UART
    if (in_commblink)
    {
        in_commblink = 0;
        commblink_start = ms;
        digitalWrite(comm, HIGH);
    }
    if (ms - commblink_start > 50)
    {
        digitalWrite(comm, LOW);
    }

    //error LED is off normally
    //if connection is lost, it flashes slowly
    //for any other more serious errors, it flashes quickly
    uint8_t state;
    switch (errblink_mode)
    {
    case ERRBLINK_MODE_OFF:
        state = 0;
        break;
    case ERRBLINK_MODE_SLOW:
        state = (ms % 1000) < 500;
        break;
    case ERRBLINK_MODE_FAST:
        state = (ms % 200) < 100;
        break;
    }
    digitalWrite(err, state);
}
void StatusLights::rx_msg_blink()
{
    in_commblink = 1;
}
void StatusLights::err_blink(uint8_t errblink_mode)
{
    this->errblink_mode = errblink_mode;
}