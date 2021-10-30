#include <thermocouple.h>
#include <SPI.h>

void Thermocouple::begin()
{
    //chip deselect
    digitalWrite(cs_pin, HIGH);
    pinMode(cs_pin, OUTPUT);
    //initialize SPI object
    SPI.begin();
}

void Thermocouple::update()
{
    //bitmap
    //D31-D18: 14-bit thermocouple temperature data
    //D17: Reserved
    //D16: Fault bit
    //D15-D4: 12-bit internal temperature data
    //D3: reserved
    //D2: SCV, D1: SCG, D0: OC

    //Set memory to zeros to avoid confusion from last message
    uint32_t rawData = 0;
    uint32_t rawInternal = 0;
    int32_t rawTC = 0;

    digitalWrite(cs_pin, LOW);          //select chip
    SPI.beginTransaction(SPI_SETTINGS); //start SPI comms

    //shift in 4 bytes to the uint32_t rawData
    rawData = SPI.transfer(0);
    rawData <<= 8;
    rawData |= SPI.transfer(0);
    rawData <<= 8;
    rawData |= SPI.transfer(0);
    rawData <<= 8;
    rawData |= SPI.transfer(0);

    SPI.endTransaction();       //stop SPI comms
    digitalWrite(cs_pin, HIGH); //deselect chip

#ifdef DEBUG
    Serial.print("DATA: ");
    Serial.println(rawData, BIN); //print 32-bit raw data in binary format
#endif

    errcode = (uint8_t)(rawData & 0b111); //this line implicitly clears all previous errors
#ifdef DEBUG
    if (errcode & 0b111)
    {
        Serial.println("Vcc GND Open");
        Serial.print(errcode >> TC_ERR_VCC_SHORT & 1);
        Serial.print(errcode >> TC_ERR_GND_SHORT & 1);
        Serial.println(errcode >> TC_ERR_OPEN_CKT & 1);
    }
#endif

    if (rawData == 0)
    {
        //could occur when things are OK
        //for case with no errors, 0degC external, 0degC internal (this last unlikely :)
        errcode |= 1 << TC_ERR_ALL_ZEROS;
    }
    else if (rawData == 0xFFFF)
    {
        errcode |= 1 << TC_ERR_ALL_ONES;
    }

    //create the 12-bit internal temperature number
    rawInternal = rawData >> 4; //shifts out the D0-D3
    //This is the "uglier solution"
    //rawInternal = rawInternal << 20; //shifts out the D17-D31
    //rawInternal = rawInternal >> 20; //places the number to its correct place

    //Alternative / nicer solution
    rawInternal = rawInternal & 4095; //4095 = B00000000000000000000111111111111

    //Calculate the temperature in Celsius
    internal_temp = rawInternal * 0.0625; //datasheet page 4
#ifdef DEBUG
    Serial.print("Internal temp: ");
    Serial.println(internal_temp); //print 12-bit Celsius data
#endif
    if (internal_temp < 10 || internal_temp > 30)
    { //room temperature values
        errcode |= 1 << TC_ERR_INTERNAL_TEMP_WILD;
    }

    //create the 14-bit (signed) thermocouple number
    rawTC = (int32_t)(rawData >> 18); //shifts out the D0-D17
#ifdef DEBUG
    Serial.print("Raw thermocouple 14-bit temp: ");
    Serial.println(rawTC); //print 14-bit raw data
#endif
    // int32_t x = rawTC;
    //taking care of the sign
    if (rawTC >> 13 == 1) //of the 14th bit is 1
    {
        rawTC -= 16384; //conversion of negative numbers
    }
    //Serial.println(x);

    //Calculating the temperature in Celsius
    external_temp_raw = (rawTC)*0.25; //datasheet page 4
    if (external_temp_raw > 50 || external_temp_raw < -270)
    { //what should lower bound be?
        errcode = 1 << TC_ERR_EXTERNAL_TEMP_WILD;
    }
#ifdef DEBUG
    Serial.print("Ext temp raw: ");
    Serial.println(external_temp_raw);
#endif

    //fix linear approximation failure of amplifier
    calculate_corrected_temp();
#ifdef DEBUG
    Serial.print("errcode:");
    Serial.println(errcode, HEX);
    Serial.println();
#endif
}

void Thermocouple::calculate_corrected_temp()
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
    if (std_ref_uV <= 0)
    { //negative temps.
        external_temp =
            2.5949192E-02 * std_ref_uV +
            -2.1316967E-07 * pow(std_ref_uV, 2.0) +
            7.9018692E-10 * pow(std_ref_uV, 3.0) +
            4.2527777E-13 * pow(std_ref_uV, 4.0) +
            1.3304473E-16 * pow(std_ref_uV, 5.0) +
            2.0241446E-20 * pow(std_ref_uV, 6.0) +
            1.2668171E-24 * pow(std_ref_uV, 7.0);
    }
    else
    { //positive temps.
        external_temp = 2.592800E-02 * std_ref_uV +
                        -7.602961E-07 * pow(std_ref_uV, 2.0) +
                        4.637791E-11 * pow(std_ref_uV, 3.0) +
                        -2.165394E-15 * pow(std_ref_uV, 4.0) +
                        6.048144E-20 * pow(std_ref_uV, 5.0) +
                        -7.293422E-25 * pow(std_ref_uV, 6.0);
    }
#ifdef DEBUG
    Serial.print("Sensor uV: ");
    Serial.println(actual_uV);
    Serial.print("ref uV: ");
    Serial.println(ref_uV);
    Serial.print("std ref uV: ");
    Serial.println(std_ref_uV);
    Serial.print("corrected temp:");
    Serial.println(external_temp);
#endif
}

float Thermocouple::get_raw_external_temp()
{
    return external_temp_raw;
}

float Thermocouple::get_corrected_external_temp()
{
    return external_temp;
}

float Thermocouple::get_internal_temp()
{
    return internal_temp;
}

uint8_t Thermocouple::get_errcode()
{
    return errcode;
}

void Thermocouple::print()
{
    Serial.print("INT");
    Serial.print(internal_temp);
    Serial.print("\tEXTRAW");
    Serial.print(external_temp_raw);
    Serial.print("\tEXTCORR");
    Serial.print(external_temp);
    Serial.print("\tERR");
    Serial.println(errcode, HEX);
}