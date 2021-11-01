//libraries
#include <Arduino.h>
#include "thermocouple.h"
#include "heater.h"

//settings
#define TC_UPDATE_PERIOD 1000

//pin config
#define TC_A_CS_PIN 10
#define TC_B_CS_PIN 9
#define SPI_CLK 13
#define SPI_MISO 12

#define HEAT_A_PIN 8
#define HEAT_B_PIN 7

#define COMM_PIN 6
#define ERR_PIN 5
#define SUPPLY_SENSE_PIN A0 //voltage divider w/ Zener protection to read power supply

//global variables
Thermocouple tc_A(TC_A_CS_PIN);
Thermocouple tc_B(TC_B_CS_PIN);

Heater h_A(HEAT_A_PIN);
Heater h_B(HEAT_B_PIN);

uint8_t heat_A_duty;
uint8_t estop;

void setup()
{
  Serial.begin(9600); //start serial
  Serial.println("boot");

  tc_A.begin();
  tc_B.begin();

  h_A.begin();
  h_B.begin();

  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  delay(1000); //wait for thermocouple to stabilize
}

uint32_t last_tc_update;
uint8_t goal;
void handle_estop();
void loop()
{
  handle_estop();
  if (Serial.available())
  {
    goal = Serial.parseInt();
  }
  if (millis() - last_tc_update > TC_UPDATE_PERIOD)
  {
    last_tc_update = millis();
    tc_A.update();
    delay(1);
    tc_B.update();
    //tc_A.print();
    //tc_B.print();
  }
  if (!estop)
  {
    h_A.set_duty(constrain((goal - tc_A.get_corrected_external_temp()) * 50.0, 0, 100));
    h_B.set_duty(h_A.get_duty());
  }
  Heater::update_all();
}

void handle_estop()
{
  if (!digitalRead(ESTOP_PIN))
  {
    estop = 1;
    h_A.off();
    h_B.off();
  }
}