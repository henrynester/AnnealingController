#pragma once
#include <Arduino.h>

//#define DEBUG

#define SPI_BITRATE 250000
#define SPI_SETTINGS SPISettings(SPI_BITRATE, MSBFIRST, SPI_MODE0)

#define TC_A_CS 9
#define TC_B_CS 10

//Class to interface with MAX31855T over SPI

//error flags by bit index:
//(from chip)
#define TC_ERR_OPEN_CKT 0  //Open circuit
#define TC_ERR_GND_SHORT 1 //TC short to GND
#define TC_ERR_VCC_SHORT 2 //TC short to VCC
//(software-errors)
#define TC_ERR_RESERVED 3

#define TC_ERR_ALL_ZEROS 4          //packet of all zeros received. indicates SDO line disconnect
#define TC_ERR_ALL_ONES 5           //packet of all ones received. indicates chip not interpreting CS and SCK correctly
#define TC_ERR_INTERNAL_TEMP_WILD 6 //out of range internal temp value
#define TC_ERR_EXTERNAL_TEMP_WILD 7 //out of range external temp value

class Thermocouple
{
private:
    uint8_t cs_pin;
    float internal_temp;
    float external_temp_raw;
    float external_temp;
    uint8_t errcode;
    void calculate_corrected_temp();

public:
    Thermocouple(uint8_t cs) : cs_pin(cs){};
    void begin();
    void update();
    float get_internal_temp();
    float get_raw_external_temp();
    float get_corrected_external_temp();
    uint8_t get_errcode();
    void print();
};