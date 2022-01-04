#pragma once
#include <Arduino.h>

#define ERRBLINK_MODE_OFF 0
#define ERRBLINK_MODE_SLOW 1
#define ERRBLINK_MODE_FAST 2

void leds_begin();
void leds_update(uint32_t ms);
void leds_rx_msg_blink();
void leds_set_errblink_mode(uint8_t mode);