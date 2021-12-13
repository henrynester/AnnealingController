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

#define MANUAL_SW 9