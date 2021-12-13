#include <thermocouple.h>
#include <SPI.h>

uint8_t Thermocouple::instance_count = 0, Thermocouple::instance_to_update = 0;
Thermocouple *Thermocouple::instances[2] = {nullptr, nullptr};

void Thermocouple::begin()
{
    //chip deselect
    digitalWrite(cs_pin, HIGH);
    pinMode(cs_pin, OUTPUT);
    //initialize SPI object
    //SPI.begin();
    pinMode(TC_SCK, OUTPUT);
    digitalWrite(TC_SCK, LOW);
    pinMode(TC_MISO, INPUT_PULLUP);
}

float calculate_corrected_temp(float, float);

uint8_t shiftIn()
{
    uint8_t value = 0x00;
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        digitalWrite(TC_SCK, HIGH);
        delayMicroseconds(5);
        value |= digitalRead(TC_MISO) << (7 - i);
        delayMicroseconds(5);
        digitalWrite(TC_SCK, LOW);
        delayMicroseconds(10);
    }
    return value;
}

//MAX31855 datasheet: datasheets.maximintegrated.com/en/ds/MAX31855.pdf
void Thermocouple::update()
{
    //bitmap
    //D31-D18: 14-bit thermocouple temperature data
    //D17: Reserved
    //D16: Fault bit
    //D15-D4: 12-bit internal temperature data
    //D3: reserved
    //D2: short circuit to Vcc, D1: to ground, D0: open circuit

    uint32_t rawData = 0;
    uint32_t rawInternal = 0;
    int32_t rawTC = 0;

    digitalWrite(cs_pin, LOW); //select chip
    delayMicroseconds(10);
    //SPI.beginTransaction(SPI_SETTINGS); //start SPI comms

    //shift in 4 bytes to the uint32_t rawData
    rawData = shiftIn(); //SPI.transfer(0);
    rawData <<= 8;
    rawData |= shiftIn(); //SPI.transfer(0);
    rawData <<= 8;
    rawData |= shiftIn(); //SPI.transfer(0);
    rawData <<= 8;
    rawData |= shiftIn(); //SPI.transfer(0);

    SPI.endTransaction();       //stop SPI comms
    digitalWrite(cs_pin, HIGH); //deselect chip

    errcode = (uint8_t)(rawData & 0b111); //clear any errors from previous calls

    if (rawData == 0)
    {
        //could occur when things are OK
        //for case with no errors, 0degC external, 0degC internal (this last unlikely :)
        errcode |= 1 << TC_ERR_ALL_ZEROS;
    }
    if (rawData == 0xFFFF)
    {
        errcode |= 1 << TC_ERR_ALL_ONES;
    }

    //extract the 12-bit internal temperature ADC value
    rawInternal = rawData >> 4;       //shifts out the D0-D3
    rawInternal = rawInternal & 4095; //keep only D4-D15

    //Calculate the temperature in Celsius
    internal_temp = rawInternal * 0.0625; //datasheet page 4 gives this as the degC/LSB

    if (internal_temp < 10 || internal_temp > 30)
    { //room temperature values
        errcode |= 1 << TC_ERR_INTERNAL_TEMP_WILD;
    }

    //create the 14-bit (signed) thermocouple ADC value
    rawTC = (int32_t)(rawData >> 18); //shifts out the D0-D17 to keep only D18-31

    //MSB (14th bit) = 1 means number is negative
    //14 zeroes -> 0
    //14 ones -> -1
    //so subtract 2^14 to convert negative numbers
    if (rawTC >> 13 == 1)
    {
        rawTC -= 16384;
    }

    //Calculating the temperature in Celsius
    external_temp_raw = (rawTC)*0.25; //datasheet page 4: 0.25degC/LSB external
    if (external_temp_raw > 50 || external_temp_raw < -270)
    { //what should lower bound be?
        errcode = 1 << TC_ERR_EXTERNAL_TEMP_WILD;
    }

    //fix linear approximation failure of amplifier
    calculate_corrected_temp(external_temp_raw, internal_temp);

#ifdef TC_DEBUG
    Serial.print("DATA: ");
    Serial.println(rawData, BIN); //print 32-bit raw data in binary format
    Serial.print("Internal temp: ");
    Serial.println(internal_temp); //print 12-bit Celsius data
    Serial.print("Raw thermocouple 14-bit temp: ");
    Serial.println(rawTC); //print 14-bit raw data
    Serial.print("Ext temp raw: ");
    Serial.println(external_temp_raw);
    Serial.print("errcode:");
    Serial.println(errcode, HEX);
    Serial.println();
#endif
}

//ITS-90 coefficients for T-type: www2.me.rochester.edu/courses/ME241/Thermocouple%20conversions.pdf
inline float calculate_corrected_temp(float external_temp_raw, float internal_temp)
{
    //coefficient is from datasheet Table 1
    //assume it's the first sensitivity number
    //by analogy to formula given for type K thermocouples
    //this is our best guess at the voltage the amp is receiving
    float actual_uV = 52.18 * (external_temp_raw - internal_temp);

    //calculate the voltage theoretically developed by a thermocouple
    //at the current reference temp (amp internal temp),
    //if it were referenced to 0degC.
    //uses ITS-90 coefficients for type T
    float ref_uV = 3.8748106364E+01 * internal_temp +
                   3.3292227880E-02 * pow(internal_temp, 2.0) +
                   2.0618243404E-04 * pow(internal_temp, 3.0) +
                   -2.1882256846E-06 * pow(internal_temp, 4.0) +
                   1.0996880928E-08 * pow(internal_temp, 5.0) +
                   -3.0815758772E-11 * pow(internal_temp, 6.0) +
                   4.5479135290E-14 * pow(internal_temp, 7.0) +
                   -2.7512901673E-17 * pow(internal_temp, 8.0);

    //calculate voltage theoretically developed by a thermocouple at our external
    //temp, but referenced to 0degC
    float std_ref_uV = actual_uV + ref_uV;

    //use the inverse ITS-90 coefficients for type T to get a temperature out
    float external_temp;
    if (std_ref_uV <= 0)
    { //negative temps have one polynomial fit
        external_temp = 2.5949192E-02 * std_ref_uV +
                        -2.1316967E-07 * pow(std_ref_uV, 2.0) +
                        7.9018692E-10 * pow(std_ref_uV, 3.0) +
                        4.2527777E-13 * pow(std_ref_uV, 4.0) +
                        1.3304473E-16 * pow(std_ref_uV, 5.0) +
                        2.0241446E-20 * pow(std_ref_uV, 6.0) +
                        1.2668171E-24 * pow(std_ref_uV, 7.0);
    }
    else
    { //positive temps have another
        external_temp = 2.592800E-02 * std_ref_uV +
                        -7.602961E-07 * pow(std_ref_uV, 2.0) +
                        4.637791E-11 * pow(std_ref_uV, 3.0) +
                        -2.165394E-15 * pow(std_ref_uV, 4.0) +
                        6.048144E-20 * pow(std_ref_uV, 5.0) +
                        -7.293422E-25 * pow(std_ref_uV, 6.0);
    }
#ifdef TC_DEBUG
    Serial.print("Sensor uV: ");
    Serial.println(actual_uV);
    Serial.print("ref uV: ");
    Serial.println(ref_uV);
    Serial.print("std ref uV: ");
    Serial.println(std_ref_uV);
    Serial.print("corrected temp:");
    Serial.println(external_temp);
#endif
    return external_temp;
}

float Thermocouple::get_raw_temp()
{
    return external_temp_raw;
}

float Thermocouple::get_temp()
{
    return external_temp;
}

float Thermocouple::get_internal_temp()
{
    return internal_temp;
}

uint8_t Thermocouple::get_errcode()
{
    return 0; //errcode;
}

// void Thermocouple::print()
// {
//     Serial.print("INT");
//     Serial.print(internal_temp);
//     Serial.print("\tEXTRAW");
//     Serial.print(external_temp_raw);
//     Serial.print("\tEXTCORR");
//     Serial.print(external_temp);
//     Serial.print("\tERR");
//     Serial.println(errcode, HEX);
// }

bool Thermocouple::update_all(uint16_t ms)
{
    //top of the period to ya
    if (ms < TC_UPDATE_GAP && instance_to_update == instance_count + 1)
    {
        instance_to_update = 0;
    }
    //space updates apart by TC_UPDATE_GAP
    if (ms > (TC_UPDATE_GAP * instance_to_update))
    {
        if (instance_to_update < instance_count)
        { //still a valid instance number
//update that thermocouple
#ifdef TC_TEST_POT
            instances[instance_to_update]->debug_set_external_temp(
                map(analogRead((instance_to_update == 0) ? A0 : A1), //A0 is TCA, A1 is TCB
                    0, 1023, -273, 25));
#else
            // if (instance_to_update == 1)
            // {
#ifdef TC_DEBUG
            Serial.print("TC:");
            Serial.println(instance_to_update);
#endif
            //if (instance_to_update == 1)
            //{
            instances[instance_to_update]->update();
            //}
#endif
            instance_to_update++;
        }
        if (instance_to_update == instance_count)
        { //updated all TCs
            instance_to_update++;
            return true;
        }
    }
    return false;
}

void Thermocouple::debug_set_external_temp(float temp)
{
    external_temp = temp;
    internal_temp = 25; //avoid the error from wild internal temp when we're just debugging
}