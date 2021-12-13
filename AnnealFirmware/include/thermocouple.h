#pragma once
#include <Arduino.h>
#include "pins.h"

#define TC_DEBUG //print out raw bytes received from sensor, conversion maths
#define TC_TEST_POT //use a potentiometer instead

#define SPI_BITRATE 125000
#define SPI_SETTINGS SPISettings(SPI_BITRATE, MSBFIRST, SPI_MODE0)

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

#define TC_UPDATE_PERIOD 1000 //time interval between starting a thermocouple read cycle
//time interval between reading thermocouples (conversions occur right after SPI communications stop)
//so we don't want to disturb the bus talking to the next sensor until that time is up.
//datasheet specifies 100ms for all conversions to complete
#define TC_UPDATE_GAP 200
class Thermocouple
{
private:
    uint8_t cs_pin;
    float internal_temp;
    float external_temp_raw;
    float external_temp;
    uint8_t errcode;
    static Thermocouple *instances[2];
    static uint8_t instance_count, instance_to_update;

public:
    Thermocouple(uint8_t cs) : cs_pin(cs)
    {
        //add a pointer to the newly-constructed Thermocouple object to the instance list
        instances[instance_count] = this;
        instance_count++;
    };
    void begin();
    void update();
    void debug_set_external_temp(float temp); //used for debugging with potentiometers
    float get_internal_temp();
    float get_raw_temp();
    float get_temp();
    uint8_t get_errcode();
    // void print();

    static bool update_all(uint16_t ms); //returns true if one was updated
};