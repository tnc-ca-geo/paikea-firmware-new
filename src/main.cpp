#include <Arduino.h>
#include <Wire.h>

// see https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout/arduino-wiring-and-test
void tcaselect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(0x24);
  Wire.write(1 << i);
  Wire.endTransmission();
}

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


// see https://www.ti.com/lit/ds/symlink/tca9535.pdf
uint config_values[8] = {
  0b00000000,  // Input register level port 0 (read only)
  0b00000000,  // Input register level port 1 (read only)
  // write output to those registers
  0b11111111,  // Output register level port 0
  0b11111111,  // Output register level port 1
  0b00000000,  // Pin polarity port 0
  0b00000000,  // Pin polarity port 1
  0b00111100,  // IO direction 1 ... input, 0 ... output port 0
  0b01110000   // IO direction 1 ... input, 0 ... output port 1
};


void setup() {
  Wire.begin();
  Serial.begin(115200);

  for (uint i=0; i<8; i++) {
    Wire.beginTransmission(0x24);
    Wire.write(i);
    Wire.write(config_values[i]);
    Wire.endTransmission();
  }
}

void loop() {
  Wire.beginTransmission(0x24);
  Wire.write(3);
  Wire.write(0b00000000);
  Wire.endTransmission();
  delay(1000);
  Wire.beginTransmission(0x24);
  Wire.write(3);
  Wire.write(0b00000001);
  Wire.endTransmission();
  delay(1000);
}