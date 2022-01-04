#include "leds.h"
#include "pins.h"

uint8_t in_commblink;
uint8_t errblink_mode;
uint32_t commblink_start;

void leds_begin()
{
    //set LED pins as outputs
    pinMode(COMM, OUTPUT);
    pinMode(ERR, OUTPUT);
    //sexy startup blink to show the MCU works
    for (uint8_t i = 0; i < 10; i++)
    {
        digitalWrite(ERR, LOW);
        digitalWrite(COMM, HIGH);
        delay(50);
        digitalWrite(COMM, LOW);
        digitalWrite(ERR, HIGH);
        delay(50);
    }
}

void leds_update(uint32_t ms)
{
    //brief flash of COMM LED on each message received over the UART
    if (in_commblink)
    {
        in_commblink = 0;
        commblink_start = ms;
        digitalWrite(COMM, HIGH);
    }
    if (ms - commblink_start > 50)
    {
        digitalWrite(COMM, LOW);
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
    digitalWrite(ERR, state);
}
void leds_rx_msg_blink()
{
    in_commblink = 1;
}
void leds_set_errblink_mode(uint8_t mode)
{
    errblink_mode = mode;
}