//libraries
#include <Arduino.h>
#include "thermocouple.h"
#include "heater.h"
#include "leds.h"
#include "AutoPID.h"

//settings
#define TC_UPDATE_PERIOD 1000

//pin config
#define TC_A_CS 10 //thermocouple A chip select (active LOW)
#define TC_B_CS 9  //B
#define TC_SCK 13  //thermocouple SPI clock
#define TC_MISO 12 //thermocouple SPI data output

#define HT_A_SW 5  //switches heater A MOSFET on when HIGH
#define HT_A_SNS 8 //HIGH when heater A + terminal has +24V on it
#define HT_B_SW 6  //B MOSFET
#define HT_B_SNS 7 //B sense

#define COMM 3 //communication (green) LED
#define ERR 4  //error (red) LED

//global variables
Thermocouple tc_A(TC_A_CS);
Thermocouple tc_B(TC_B_CS);

Heater ht_A(HT_A_SW, HT_A_SNS);
Heater ht_B(HT_B_SW, HT_B_SNS);

StatusLights leds(COMM, ERR);

uint8_t heat_A_duty;
uint8_t estop;

float goal_temp = -50.0;
float Kp = 1.0, Ki = 0.000562, Kd = -125892012.3430;

void setup()
{
  Serial.begin(250000); //start serial
  Serial.println("boot");

  tc_A.begin();
  tc_B.begin();

  ht_A.begin();
  ht_B.begin();

  //run this last
  leds.begin(); //startup blink gives ~1000ms time for TC amps to stabilize
}

uint32_t ms;
uint32_t last_tx;
uint32_t last_rx;
void loop()
{
  ms = millis();
  Thermocouple::update_all(ms);
  Heater::update_all(ms);
  leds.update(ms);
  if (ms - last_tx > 1000)
  {
    transmit_status;
    last_tx = ms;
  }
  if (Serial.available())
  {
    leds.err_blink(ERRBLINK_MODE_OFF);
    last_rx = ms;
    leds.rx_msg_blink();
    while (Serial.available())
    {
      Serial.read();
    }
  }

  if (tc_A.get_errcode() || tc_B.get_errcode() || !ht_A.has_power() || !ht_B.has_power())
  {
    leds.err_blink(ERRBLINK_MODE_FAST);
  }
  else if (ms - last_rx > 5000)
  {
    leds.err_blink(ERRBLINK_MODE_SLOW);
  }
}

#define COMMA() Serial.print(",") //save typing
void transmit_status()
{
  //sends status data as csv list
  Serial.print(millis()); //uptime
  COMMA();
  Serial.print(goal_temp); //setpoint temperature for both heater/thermocouple pairs
  COMMA();

  //thermocouple status and data
  Serial.print(tc_A.get_temp());
  COMMA();
  Serial.print(tc_B.get_temp());
  COMMA();
  Serial.print(tc_A.get_internal_temp());
  COMMA();
  Serial.print(tc_B.get_internal_temp());
  COMMA();
  Serial.print(tc_A.get_errcode());
  COMMA();
  Serial.print(tc_B.get_errcode());
  COMMA();

  //heater status and output levels
  Serial.print(ht_A.get_duty());
  COMMA();
  Serial.print(ht_B.get_duty());
  COMMA();
  Serial.print(ht_A.has_power());
  COMMA();
  Serial.print(ht_B.has_power());
  COMMA();

  //current PID gains
  Serial.print(Kp);
  COMMA();
  Serial.print(Ki);
  COMMA();
  Serial.print(Kd);
  COMMA();
  Serial.println(); //newline at end of packet
}

//serial commands:
//reboot
//try new PID gains
//write PID gains to eeprom
//set temperature
//emergency stop