#include "comms.h"
#include <avr/wdt.h> //for reset
#include <EEPROM.h>  //for storing PID params
#include "AutoPID.h"

extern float goal_temp, Kp, Ki, Kd;
extern uint8_t estop, rx_flag;
extern AutoPID pid_A, pid_B;

char rxbuf[RXBUF_LEN]; //stores received character string
char cmd[4] = {0};

//serial commands:
//<SET,100.0> set temperature
//<OFF> emergency stop
//<RST> reboot
//<PID,1.0,2.2,0.35> try new PID gains
//<SAV> write PID gains to eeprom

bool parse_rx()
{
    char *ptr; // this is used by strtok() as an index

    ptr = strtok(rxbuf, ","); //replace commas with null terminators, then return ptr to first part (cmd)
    if (strlen(ptr) != 3)     //cmd strings must be 3 chars
    {
        return false;
    }
    strcpy(cmd, ptr); // copy it to the cmd string

    //take different action depending on cmd
    if (strcmp(cmd, "SET") == 0)
    {
        ptr = strtok(NULL, ","); // NULL arg continues using old string and moves ptr ahead
        goal_temp = atof(ptr);   // convert next part to a float
        estop = 0;
#ifdef COMMS_DEBUG
        Serial.println(goal_temp);
#endif
        return true;
    }
    else if (strcmp(cmd, "OFF") == 0)
    {
        estop = 1;
        return true;
    }
    else if (strcmp(cmd, "RST") == 0)
    {
        //reset the microcontroller
        wdt_enable(WDTO_15MS); //set watchdog to expire in 15ms and reset the MCU
        while (1)              //execute NOP until that happens
            ;
        return true; //never reached. avoids warnings.
    }
    else if (strcmp(cmd, "PID") == 0)
    {
        //use new PID parameters
        ptr = strtok(NULL, ",");
        Kp = atof(ptr);
        ptr = strtok(NULL, ",");
        Ki = atof(ptr);
        ptr = strtok(NULL, ",");
        Kd = atof(ptr);
#ifdef COMMS_DEBUG
        Serial.println(Kp);
        Serial.println(Ki);
        Serial.println(Kd);
#endif
        pid_A.setGains(Kp, Ki, Kd);
        pid_B.setGains(Kp, Ki, Kd);
        return true;
    }
    else if (strcmp(cmd, "SAV") == 0)
    {
        //write PID params to EEPROM
        uint8_t addr = 0;
        EEPROM.put(addr, Kp);
        addr += sizeof(float);
        EEPROM.put(addr, Ki);
        addr += sizeof(float);
        EEPROM.put(addr, Kd);
        return true;
    }
    else
    {
        return false; //did not match any of the valid commands
    }
}

#define STARTMARKER '<'
#define ENDMARKER '>'
void serial_rx()
{
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char rc;

    while (Serial.available() > 0 && !rx_flag)
    {
        rc = Serial.read();

        if (recvInProgress == true)
        {
            if (rc != ENDMARKER)
            {
                rxbuf[ndx] = rc;
                ndx++;
                if (ndx >= RXBUF_LEN)
                {
                    ndx = RXBUF_LEN - 1;
                }
            }
            else
            {
                rxbuf[ndx] = '\0'; // terminate char array
                recvInProgress = false;
                ndx = 0;
#ifdef COMMS_DEBUG
                Serial.println(rxbuf);
#endif
                rx_flag = 1;
            }
        }

        else if (rc == STARTMARKER)
        {
            recvInProgress = true;
        }
    }
}
