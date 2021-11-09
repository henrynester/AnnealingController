//libraries
#include <Arduino.h>
#include <EEPROM.h> //for storing PID params
#include "thermocouple.h"
#include "heater.h"
#include "leds.h"
#include "AutoPID.h"
#include "comms.h"

//settings
extern const uint16_t LOOP_PERIOD;
const uint16_t LOOP_PERIOD = 1000;
#define COMMS_TIMEOUT 10000

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

float goal_temp = -50.0;
float Kp, Ki, Kd;
float temp_A, temp_B, duty_A, duty_B;
AutoPID pid_A(&temp_A, &goal_temp, &duty_A, 0, 100, 0, 0, 0);
AutoPID pid_B(&temp_B, &goal_temp, &duty_B, 0, 100, 0, 0, 0);

StatusLights leds(COMM, ERR);

uint8_t heat_A_duty;
uint8_t estop = 1;

bool errchk();

void setup()
{
  Serial.begin(250000); //start serial
  Serial.println("boot");

  tc_A.begin();
  tc_B.begin();

  ht_A.begin();
  ht_B.begin();

  //load PID gains from EEPROM
  EEPROM.get(0, Kp);
  EEPROM.get(sizeof(float), Ki);
  EEPROM.get(sizeof(float) * 2, Kd);
  if (Kp == NAN || Ki == NAN || Kd == NAN)
  {
    Kp = Ki = Kd = 0.0;
  }
  pid_A.setGains(Kp, Ki, Kd);
  pid_B.setGains(Kp, Ki, Kd);

  //run this last
  leds.begin(); //startup blink gives ~1000ms time for TC amps to stabilize
}

uint32_t ms;
uint16_t t;
uint32_t last_period_start;
uint32_t last_rx = 0;
uint8_t comms_ok = 0;
uint8_t rx_flag;

//todo: global 1Hz period variable
void loop()
{
  //avoid repeated calls; we have to turn of interrupts in there
  ms = millis();
  //t = how far we are into current period, in milliseconds
  t = ms - last_period_start;
  if (t >= LOOP_PERIOD)
  {
    serial_tx(); //transmit status 1Hz
    error_tx();
    last_period_start = ms; //restart the period
  }
  if (Thermocouple::update_all(t))
  { //only re-run PID loops if there is a new temp. reading
    if (!estop)
    {
      pid_A.run();
      pid_B.run();
    }
  }
  if (estop)
  {
    Heater::shutdown_all();
  }
  else
  {
    ht_A.set_duty(duty_A);
    ht_B.set_duty(duty_B);
  }

  Heater::update_all(t);

  leds.update(t);

  serial_rx();

  if (rx_flag) //msg received
  {
    rx_flag = 0;

    if (parse_rx())
    { //msg was valid
      comms_ok = 1;
      last_rx = ms;        //restart the timeout
      leds.rx_msg_blink(); //blink the COMM led
    }
  }

  //check for comms timeout
  if (comms_ok && (ms - last_rx) > COMMS_TIMEOUT)
  {
    comms_ok = 0;
    estop = 1; //stop heaters on comms lost
  }

  //blink LED fast for errors
  if (errchk())
  {
    estop = 1;
    leds.err_blink(ERRBLINK_MODE_FAST);
  }
  //blink LED slow if connection lost
  else if (!comms_ok)
  {
    leds.err_blink(ERRBLINK_MODE_SLOW);
  }
  else
  {
    leds.err_blink(ERRBLINK_MODE_OFF);
  }
}

#define COMMA() Serial.print(',') //save typing
void serial_tx()
{
  Serial.print("<DAT,");
  //sends status data as csv list
  Serial.print(millis() / 1000.0); //uptime
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
  Serial.print(Kp, 5);
  COMMA();
  Serial.print(Ki, 5);
  COMMA();
  Serial.print(Kd, 5);
  Serial.println('>'); //newline at end of packet
}

bool errchk()
{
  if (tc_A.get_errcode() || tc_B.get_errcode() || !ht_A.has_power() || !ht_B.has_power())
  {
    return true;
  }
  else
  {
    return false;
  }
}

void error_tx()
{
  if (errchk())
  {
    Serial.print("<ERR,");
    Serial.print(tc_A.get_errcode(), HEX);
    COMMA();
    Serial.print(tc_B.get_errcode(), HEX);
    COMMA();
    if (!ht_A.has_power())
    {
      Serial.print('A');
    }
    if (!ht_B.has_power())
    {
      Serial.print('B');
    }
    Serial.println(">");
  }
}