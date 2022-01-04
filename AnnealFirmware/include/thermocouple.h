#pragma once
#include <Arduino.h>
#include "pins.h"

//Class to interface with ADS1120 over SPI

//#define TC_DEBUG //uncomment to print out raw bytes received from sensor, conversion maths

void adc_init();

#define ADC_CHANNEL_INTERNAL_TEMP 1
#define ADC_CHANNEL_TC_A 2
#define ADC_CHANNEL_TC_B 3
void adc_select_channel(uint8_t channel);
void adc_start_conversion();
bool adc_is_conversion_ready();
int16_t adc_read_conversion();

float adc_to_internal_temp(int16_t adc);
float adc_to_thermocouple_temp(int16_t adc, float internal_temp);

//error flags by bit index:
#define ADC_ERR_BAD_SPI 0x01            //config register write failed OR called
#define ADC_ERR_INTERNAL_TEMP_WILD 0x02 //out of range internal temp value
#define ADC_ERR_TEMP_A_WILD 0x04        //A channel is reading unreasonably high or low
#define ADC_ERR_TEMP_B_WILD 0x08        //B channel
uint8_t adc_get_errcode();