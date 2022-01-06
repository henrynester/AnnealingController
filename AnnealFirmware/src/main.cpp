// //libraries
#include <Arduino.h>
#include <EEPROM.h> //for storing PID params
#include "thermocouple.h"
#include "heater.h"
#include "leds.h"
#include "AutoPID.h"
#include "comms.h"
#include "manual.h"
#include "pins.h"
#include <avr/wdt.h>

//settings
extern const uint16_t LOOP_PERIOD;
const uint16_t LOOP_PERIOD = 1000;
#define COMMS_TIMEOUT 10000

//global variables
Heater ht_A(HT_A_SW, HT_A_SNS);
Heater ht_B(HT_B_SW, HT_B_SNS);

float goal_temp = -50.0;
float internal_temp; //ADC (cold-junction) temperature
float Kp, Ki, Kd;
float temp_A, temp_B, duty_A, duty_B;
AutoPID pid_A(&temp_A, &goal_temp, &duty_A, 0, 100, 0, 0, 0);
AutoPID pid_B(&temp_B, &goal_temp, &duty_B, 0, 100, 0, 0, 0);

uint8_t heat_A_duty;
uint8_t estop = 1;
uint8_t error = 0;

bool errchk();

#define STATE_CONVERT_INTERNAL 1
#define STATE_CONVERT_TC_A 2
#define STATE_CONVERT_TC_B 3
#define STATE_ADC_IDLE 4
uint8_t state = STATE_ADC_IDLE;

uint32_t ms;
uint16_t t;
uint32_t last_period_start;
uint32_t last_rx = 0;
uint8_t comms_ok = 0;
uint8_t rx_flag;

void setup()
{
  Serial.begin(250000);
  Serial.println("boot");

  adc_init();

  ht_A.begin();
  ht_B.begin();

  //load PID gains from EEPROM
  EEPROM.get(0, Kp);
  EEPROM.get(sizeof(float), Ki);
  EEPROM.get(sizeof(float) * 2, Kd);
  if (Kp == NAN || Ki == NAN || Kd == NAN)
  {
    //if the EEPROM data are corrupted anywhere, set all the gains to zero
    //to prevent any operation
    Kp = Ki = Kd = 0.0;
  }
  pid_A.setGains(Kp, Ki, Kd);
  pid_B.setGains(Kp, Ki, Kd);

  leds_begin(); //startup blink gives ~1000ms time for TC amps to stabilize

  if (check_manual_sw())
  { //switch set for MANUAL
    manual_mode();
  } //switch set for CPU => continue to main program
}

//todo: global 1Hz period variable
void loop()
{
  //avoid repeated calls to millis() since interrupts are disabled there
  ms = millis();
  //t = how far we are into current period, in milliseconds
  t = ms - last_period_start;
  switch (state)
  {
  case STATE_CONVERT_INTERNAL:
    if (adc_is_conversion_ready())
    {
      internal_temp = adc_to_internal_temp(adc_read_conversion());
      state = STATE_CONVERT_TC_A;
      adc_select_channel(ADC_CHANNEL_TC_A);
      adc_start_conversion();
    }
    break;
  case STATE_CONVERT_TC_A:

    if (adc_is_conversion_ready())
    {
      temp_A = adc_to_thermocouple_temp(adc_read_conversion(), internal_temp);
      state = STATE_CONVERT_TC_B;
      adc_select_channel(ADC_CHANNEL_TC_B);
      adc_start_conversion();
    }
    break;
  case STATE_CONVERT_TC_B:
    if (adc_is_conversion_ready())
    {
      temp_B = adc_to_thermocouple_temp(adc_read_conversion(), internal_temp);
      //recalculate PID outputs after getting new temp. readings from both sensors
      if (!estop)
      {
        pid_A.run();
        pid_B.run();
      }
      state = STATE_ADC_IDLE;
    }
    break;
  case STATE_ADC_IDLE:
    if (t >= LOOP_PERIOD)
    {
      state = STATE_CONVERT_INTERNAL;
      adc_select_channel(ADC_CHANNEL_INTERNAL_TEMP);
      adc_start_conversion();
    }
    break;
  }

  if (estop)
  {
    ht_A.shutdown();
    ht_B.shutdown();
  }
  else
  {
    ht_A.set_duty(duty_A);
    ht_B.set_duty(duty_B);
  }

  ht_A.update(t);
  ht_B.update(t);

  leds_update(t);

  serial_rx();

  if (rx_flag) //msg received
  {
    rx_flag = 0;

    if (parse_rx())
    { //msg was valid
      comms_ok = 1;
      last_rx = ms;        //restart the timeout
      leds_rx_msg_blink(); //blink the COMM led

      //even if we received a <SET,XXX> message
      //do not leave emergency stop if the system
      //encountered an error
      if (error)
      {
        estop = 1;
      }
    }
  }

  //check for comms timeout
  if (comms_ok && (ms - last_rx) > COMMS_TIMEOUT)
  {
    comms_ok = 0;
    estop = 1; //stop heaters on comms lost
  }

  if (t >= LOOP_PERIOD)
  {
    if (!digitalRead(MANUAL_SW))
    {
      reboot();
    }
    error = errchk();
    serial_tx(); //transmit status 1Hz
    error_tx();
    //blink LED fast for errors
    if (error)
    {
      estop = 1;
      leds_set_errblink_mode(ERRBLINK_MODE_FAST);
    }
    //blink LED slow if connection lost
    else if (!comms_ok)
    {
      leds_set_errblink_mode(ERRBLINK_MODE_SLOW);
    }
    else
    {
      leds_set_errblink_mode(ERRBLINK_MODE_OFF);
    }
    last_period_start = ms; //restart the period
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

  //thermocouple temperatures
  Serial.print(temp_A);
  COMMA();
  Serial.print(temp_B);
  COMMA();
  //internal temp. of ADC (cold-junction temp)
  Serial.print(internal_temp);
  COMMA();

  //heater status and output levels
  Serial.print(ht_A.get_duty());
  COMMA();
  Serial.print(ht_B.get_duty());
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
  return adc_get_errcode() || !ht_A.has_power() || !ht_B.has_power();
}

void error_tx()
{
  if (errchk())
  {
    Serial.print("<ERR,");
    Serial.print(adc_get_errcode(), HEX);
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