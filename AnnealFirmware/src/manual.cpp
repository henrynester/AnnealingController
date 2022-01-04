#include "manual.h"
#include "pins.h"
#include "comms.h"

bool check_manual_sw()
{
    pinMode(MANUAL_SW, INPUT_PULLUP);
    delay(5); //wait for line to stabilize
    return !digitalRead(MANUAL_SW);
}

void manual_mode()
{
    //all box functions disabled except accepting signals from manual control box
    //if a fuse blow is detected, the heaters are shut off completely.

    while (true)
    {
        if (digitalRead(HT_A_SNS) && digitalRead(HT_B_SNS))
        {                                             //both fuses OK
            digitalWrite(ERR, millis() % 2000 < 100); //slow pulse pattern on error LED
            //set MOSFET lines to high-Z so external signals override
            pinMode(HT_A_SW, INPUT);
            pinMode(HT_B_SW, INPUT);
        }
        else
        {
            //blew a fuse or lost power otherwise
            digitalWrite(ERR, millis() % 200 < 100); //fast flash pattern on error LED
            //shut off heaters
            pinMode(HT_A_SW, OUTPUT);
            pinMode(HT_B_SW, OUTPUT);
            digitalWrite(HT_A_SW, LOW);
            digitalWrite(HT_B_SW, LOW);
        }

        //escape manual mode if the switch is flipped back to CPU
        if (digitalRead(MANUAL_SW))
        {
            reboot();
        }

        delay(10);
    }
}