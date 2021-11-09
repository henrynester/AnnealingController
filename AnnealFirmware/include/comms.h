#pragma once
#include <Arduino.h>

#define RXBUF_LEN 64

void serial_tx();
void error_tx();
void serial_rx();
bool parse_rx();