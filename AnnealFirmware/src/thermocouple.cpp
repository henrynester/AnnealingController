#include <thermocouple.h>
#include <SPI.h>

#define SPI_MASTER_DUMMY 0xFF
// Commands for the ADC
#define CMD_RESET 0x07
#define CMD_START_SYNC 0x08
#define CMD_PWRDWN 0x03
#define CMD_RDATA 0x1f
#define CMD_RREG 0x20
#define CMD_WREG 0x40

// Configuration registers
#define CONFIG_REG0_ADDRESS 0x00
#define CONFIG_REG1_ADDRESS 0x01
#define CONFIG_REG2_ADDRESS 0x02
#define CONFIG_REG3_ADDRESS 0x03

// Register masks for setings
// Register 0
#define REG_MASK_MUX 0xF0
#define REG_MASK_GAIN 0x0E
#define REG_MASK_PGA_BYPASS 0x01

// Register 1
#define REG_MASK_DATARATE 0xE0
#define REG_MASK_OP_MODE 0x18
#define REG_MASK_CONV_MODE 0x04
#define REG_MASK_TEMP_MODE 0x02
#define REG_MASK_BURNOUT_SOURCES 0x01

// Register 2
#define REG_MASK_VOLTAGE_REF 0xC0
#define REG_MASK_FIR_CONF 0x30
#define REG_MASK_PWR_SWITCH 0x08
#define REG_MASK_IDAC_CURRENT 0x07

// Register 3
#define REG_MASK_IDAC1_ROUTING 0xE0
#define REG_MASK_IDAC2_ROUTING 0x1C
#define REG_MASK_DRDY_MODE 0x02
#define REG_MASK_RESERVED 0x01

uint8_t adc_errcode;

void write_register(uint8_t address, uint8_t value)
{
    SPI.transfer(CMD_WREG | (address << 2));
    SPI.transfer(value);
}

uint8_t read_register(uint8_t address)
{
    SPI.transfer(CMD_RREG | (address << 2));
    uint8_t data = SPI.transfer(SPI_MASTER_DUMMY);
    return data;
}

void send_command(uint8_t command)
{
    SPI.transfer(command);
}

#define CONFIG_REG_VALS        \
    {                          \
        0x0E, 0x00, 0x00, 0x02 \
    }
//[0] in+ is AIN0, in- is AIN1, gain=128 (max), internal PGAmp. enabled
//[1] 20SPS (min for max accuracy), normal mode, single-shot conversion mode, internal temp off, no burnout detection
//[2] internal 2.048V reference, 60Hz notch disabled (it caused read issues), lowside switch open, excitation sources off
//[3] excitation sources off, MISO signal is also used to indicate DRDY (data ready) at conversion completion

void adc_init()
{
    // pinMode(ADC_MOSI, OUTPUT);
    // pinMode(ADC_MISO_DRDY, INPUT);
    // pinMode(ADC_SCK, OUTPUT);

    // Configure the SPI interface (CPOL=0, CPHA=1)
    SPI.begin(); //old Arduino.h had pin arguments to .begin(): (ADS1120_CLK_PIN, ADS1120_MISO_PIN, ADS1120_MOSI_PIN);
    SPI.setDataMode(SPI_MODE1);

    delay(5);
    send_command(CMD_RESET); //reset the ADC in case this has not been a hard power cycle of the Arduino 5V bus
    delay(100);              //wait for device to reboot

    const uint8_t reg_vals[] = CONFIG_REG_VALS;
#ifdef TC_DEBUG
    Serial.println("ADC config registers");
#endif
    for (uint8_t i = 0; i <= 3; i++)
    {
        //set bits in ADC config registers
        write_register(i, reg_vals[i]);
        delay(5);
        //check that each register is set as expected
        //if not, this indicates a problem with the SPI communication
        uint8_t read_val = read_register(i);
        delay(5);
        if (read_val != reg_vals[i])
        {
            adc_errcode |= ADC_ERR_BAD_SPI;
#ifdef TC_DEBUG
            Serial.print(i);
            Serial.println(" INCORRECT");
#endif
        }
#ifdef TC_DEBUG
        Serial.print(i);
        Serial.print(":");
        Serial.println(read_val, HEX);
#endif
    }
}

uint8_t adc_channel;
void adc_select_channel(uint8_t channel)
{
    switch (channel)
    {
    case ADC_CHANNEL_INTERNAL_TEMP:
        write_register(CONFIG_REG1_ADDRESS, 0x02); //connect internal temperature sensor to ADC
        break;
    case ADC_CHANNEL_TC_A:
        write_register(CONFIG_REG1_ADDRESS, 0x00); //disconnect internal temperature sensor from ADC
        write_register(CONFIG_REG0_ADDRESS, 0x0E); //in+ is AIN0, in- is AIN1, gain=128 (max), internal PGAmp enabled
        break;
    case ADC_CHANNEL_TC_B:
        write_register(CONFIG_REG1_ADDRESS, 0x00); //disconnect internal temperature sensor from ADC
        write_register(CONFIG_REG0_ADDRESS, 0x5E); //in+ is AIN2, in- is AIN3, gain=128 (max), internal PGAmp enabled
        break;
    }
    adc_channel = channel; //remember which channel we're on
}

uint32_t conversion_start_time; //used to keep track of time waited
                                //for timeout purposes
void adc_start_conversion()
{
    send_command(CMD_START_SYNC);
    conversion_start_time = millis();
}

bool adc_is_conversion_ready()
{
    //when the ADC has DRDYM=1 set in the config registers,
    //the MISO line goes LOW when a conversion is ready for reading.

    //conversion should take 50ms from receiving START/SYNC
    //if it takes longer, there's an issue
    if ((millis() - conversion_start_time) > 250)
    {
#ifdef TC_DEBUG
        Serial.println("ADC timed out waiting for conversion");
#endif
        adc_errcode |= (1 << ADC_ERR_BAD_SPI); //we probably lost the SPI bus
        return true;                           //just to break out of the loop
    }
    return !digitalRead(ADC_MISO_DRDY);
}

int16_t adc_read_conversion()
{
    if (adc_is_conversion_ready())
    {
        //adc reading comes in as two bytes
        int16_t adcVal = SPI.transfer(SPI_MASTER_DUMMY);
        adcVal = (adcVal << 8) | SPI.transfer(SPI_MASTER_DUMMY);
        return adcVal;
    }
    else
    {
        //function called while DRDY is HIGH, indicating no data ready to read
        return 0; //idk
    }
}

float adc_to_internal_temp(int16_t adc)
{
    float temp = (adc >> 2) * 0.03125; //temperature is a left-justified 14-bit value. LSB=0.03125degC
    //we do not account for the possibility of ADC temperatures below 0degC here
    if (temp < 3 || temp > 35)
    { //35-95degF ADC temperatures are reasonable room temperatures
        adc_errcode |= ADC_ERR_INTERNAL_TEMP_WILD;
        return 0;
    }
    else
    {
        adc_errcode &= ~ADC_ERR_INTERNAL_TEMP_WILD;
        return temp;
    }
}

float adc_to_thermocouple_temp(int16_t adc, float internal_temp)
{
    //from the datasheet, the LSB=2*Vref/gain/2^16, where Vref=2.048V and gain=128
    //calculate the thermocouple probe reading, in microvolts
    float tc_uV = adc * 0.48828125;

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
    float std_tc_uV = tc_uV + ref_uV;

    //use the inverse ITS-90 coefficients for type T to get a temperature out
    float external_temp;
    if (std_tc_uV <= 0)
    { //negative temps have one polynomial fit
        external_temp = 2.5949192E-02 * std_tc_uV +
                        -2.1316967E-07 * pow(std_tc_uV, 2.0) +
                        7.9018692E-10 * pow(std_tc_uV, 3.0) +
                        4.2527777E-13 * pow(std_tc_uV, 4.0) +
                        1.3304473E-16 * pow(std_tc_uV, 5.0) +
                        2.0241446E-20 * pow(std_tc_uV, 6.0) +
                        1.2668171E-24 * pow(std_tc_uV, 7.0);
    }
    else
    { //positive temps have another
        external_temp = 2.592800E-02 * std_tc_uV +
                        -7.602961E-07 * pow(std_tc_uV, 2.0) +
                        4.637791E-11 * pow(std_tc_uV, 3.0) +
                        -2.165394E-15 * pow(std_tc_uV, 4.0) +
                        6.048144E-20 * pow(std_tc_uV, 5.0) +
                        -7.293422E-25 * pow(std_tc_uV, 6.0);
    }
#ifdef TC_DEBUG
    Serial.println("Start TC calc");
    Serial.print("TC ADC reading: ");
    Serial.println(adc);
    Serial.print("TC uV: ");
    Serial.println(tc_uV);
    Serial.print("Internal temp: ");
    Serial.println(internal_temp);
    Serial.print("ref uV: ");
    Serial.println(ref_uV);
    Serial.print("std ref uV: ");
    Serial.println(std_tc_uV);
    Serial.print("corrected temp:");
    Serial.println(external_temp);
    Serial.println("End TC calc");
#endif
    if (external_temp < -270 || external_temp > 35)
    {
        //just above absolute zero to a hot room temperature is reasonable for the cryogenic apparatus
        if (adc_channel == ADC_CHANNEL_TC_A)
        {
            adc_errcode |= ADC_ERR_TEMP_A_WILD;
        }
        else if (adc_channel == ADC_CHANNEL_TC_B)
        {
            adc_errcode |= ADC_ERR_TEMP_B_WILD;
        }
        return 0;
    }
    else
    {
        if (adc_channel == ADC_CHANNEL_TC_A)
        {
            adc_errcode &= ~ADC_ERR_TEMP_A_WILD;
        }
        else if (adc_channel == ADC_CHANNEL_TC_B)
        {
            adc_errcode &= ~ADC_ERR_TEMP_B_WILD;
        }
        return external_temp;
    }
}

uint8_t adc_get_errcode()
{
    return adc_errcode;
}