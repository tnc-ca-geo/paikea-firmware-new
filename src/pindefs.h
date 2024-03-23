#ifndef __PIN_DEFS
#define __PIN_DEFS

// settings and pindefs
#define PORT_EXPANDER_I2C_ADDRESS 0x24
#define GPS_SERIAL_RX_PIN 35
#define GPS_SERIAL_TX_PIN 12
#define ROCKBLOCK_SERIAL_RX_PIN 34
#define ROCKBLOCK_SERIAL_TX_PIN 25
#define ROCKBLOCK_SERIAL_SPEED 115200
#define NUM_TIMERS 1
#define uS_TO_S_FACTOR 1000000
#define SLEEP_TIME 300
#define WAKE_TIME 60
// time before we decide that we won't get a fix
#define GPS_TIME_OUT 300

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
gps_extint  0     2    1
button0     0     3    1
button1     0     4    1
button2     0     5    1
unused      0     6    0
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

#endif