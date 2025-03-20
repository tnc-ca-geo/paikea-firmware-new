#ifndef __PINDEFS_H__
#define __PINDEFS_H__

// settings and pindefs
#define PORT_EXPANDER_I2C_ADDRESS 0x24
#define PORT_EXPANDER_GPS_ENABLE_PIN 0
#define PORT_EXPANDER_ROCKBLOCK_ENABLE_PIN 13
#define GPS_SERIAL_RX_PIN 35
#define GPS_SERIAL_TX_PIN 12
#define ROCKBLOCK_SERIAL_RX_PIN 34
#define ROCKBLOCK_SERIAL_TX_PIN 25
#define ROCKBLOCK_SERIAL_SPEED 19200
#define NUM_TIMERS 1

/* Pin mapping
===  ========   === ======
Pin  label      Pin label
===  ========   === ======
00   gps_enb    10  led1
01   xl_enb     11  led2
02   v5_enb     12  led3
03   button0    13  ird_enb
04   button1    14  ird_ri
05   button2    15  ird_netav
06   NC         16  ird_cts
07   led0       17  ird_rts
===  ========  === ======
*/

/* Nets
========    ====  ===  ========================
function    port  bit  config (input or output)
========    ====  ===  ========================

gps_enb     0     0    0
xl_enb      0     1    0
gps_extint  0     2    1    // What does this do?
button0     0     3    1
button1     0     4    1
button2     0     5    1
unused      0     6    0    // Used for bus check
led0        0     7    0
led1        1     0    0
led2        1     1    0
led3        1     2    0
ird_enb     1     3    0
ird_ri      1     4    1
ird_netav   1     5    1
ird_cts     1     6    1
ird_rts     1     7    0
*/


// hardware constants
#define LED01 10
#define LED00 7
// battery voltage measurement network
#define ADC_VREF 3.3
#define BATT_ADC 39
#define BATT_R_UPPER 100 // kOhm, upper R13 of voltage divider
#define BATT_R_LOWER 100 // kOhm. lower R11 of voltage divider
#define BATT_FUDGE 1.1 // everyone seems to add a fudge factor here
// constants
#define DEFAULT_INTERVAL 600
#define RETRY_INTERVAL 600
// time after which system is shutdown no matter what, 5 minutes
#define SYSTEM_TIME_OUT 300
// time before we decide that we won't get a fix
#define GPS_TIME_OUT 120
// time we wake up early (before send time)
#define GPS_DELAY 20        // IMPLEMENT
// Maximum regular reporting time, 86400s = 1day
#define MAXIMUM_INTERVAL 86400  // IMPLEMENT
// Mininmum sleep time, 5s
#define MINIMUM_SLEEP 5
// Maximum sleep time, 259200s = 3 days
#define MAXIMUM_SLEEP 259200

#endif /* __PINDEFS_H__ */
