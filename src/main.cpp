#include <Arduino.h>
#include <Wire.h>
#include <tca95xx.h>
#include <gps.h>
#include <display.h>

#define PORT_EXPANDER_I2C_ADDRESS 0x24
#define GPS_RX_PIN 35
#define GPS_TX_PIN 12

#define USE_DISPLAY 1

#if USE_DISPLAY
#endif

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
uint8_t config_values[] = {
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

// Use to protect I2C
static SemaphoreHandle_t mutex;

Gps gps=Gps();
// see https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/#:~:text=To%20use%20the%20two%20I2C%20bus%20interfaces%20of%20the%20ESP32,to%20create%20two%20TwoWire%20instances.&text=Then%2C%20initialize%20I2C%20communication%20on,pins%20with%20a%20defined%20frequency.&text=Then%2C%20you%20can%20use%20the,with%20the%20I2C%20bus%20interfaces.
// TwoWire I2Cone = TwoWire(0);
// TwoWire I2Ctwo = TwoWire(1);
Expander expander=Expander(Wire);
LilyGoDisplay display=LilyGoDisplay(Wire);


struct state {
  time_t real_time;
} state;

/*
 * My first freeRTOS task
 */
void Task_test(void *pvParameters) {
  for(;;) {
    Serial.println("Running task");
    vTaskDelay( pdMS_TO_TICKS( 5000 ) );
  }
}

/*
 * Blink LED 10. This is a useful indicator for the system running. We are need
 * a mutex here since we are using the I2C bus shared with the display.
 */
void Task_blink(void *pvParamaters) {

  for(;;) {
    xSemaphoreTake(mutex, 200);
    expander.digitalWrite(10, 1);
    xSemaphoreGive(mutex);
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
    xSemaphoreTake(mutex, 200);
    expander.digitalWrite(10, 0);
    xSemaphoreGive(mutex);
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}

// sync RTC with GPS
void Task_time_sync(void *pvParameters) {
  uint32_t sync_time;
  for(;;) {
    sync_time = (esp_timer_get_time() - gps.gps_read_system_time)/1000;
    Serial.print("Sync time in ms: "); Serial.println(sync_time);
    state.real_time = gps.epoch;
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
  }
}

// run GPS
void Task_gps(void *pvParameters) {
   for(;;) {
    gps.loop();
    vTaskDelay( pdMS_TO_TICKS( 500 ) );
  }
}

/*
 * Output part of the state to the display. We are using a mutex since we are
 * using the I2C bus.
 */
void Task_display(void *pvParameters) {
  uint32_t number = 0;
  char* time_string = new char[255];
  for(;;) {
    sprintf(time_string, "%d", number);
    xSemaphoreTake(mutex, portMAX_DELAY);
    display.set(time_string);
    xSemaphoreGive(mutex);
    number++;
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}

void setup() {
  // Serial is shared
  // TODO: wrap it in some debug code
  Serial.begin(115200);
  // I2Cone.begin(21, 22);
  Wire.begin();
  // I2Ctwo.begin(21, 22);
  mutex = xSemaphoreCreateMutex();
  delay(100);
  // TODO: move address to library since we don't really gave a choice here
  expander.begin(PORT_EXPANDER_I2C_ADDRESS);
  // Init GPS
  gps.begin(GPS_RX_PIN, GPS_TX_PIN);
  // Init display and expander, both relay on Wire.
  display.begin();
  // Arduino keywords INPUT and OUTPUT don't work here
  expander.pinMode(3, EXPANDER_INPUT);
  expander.pinMode(0, EXPANDER_OUTPUT);
  expander.digitalWrite(0, HIGH);

  xTaskCreate(&Task_test, "Task test", 4096, NULL, 3, NULL);
  xTaskCreate(&Task_blink, "Task blink", 4096, NULL, 1, NULL);
  xTaskCreate(&Task_gps, "Task gps", 4096, NULL, 2, NULL);
  xTaskCreate(&Task_time_sync, "Task time sync", 4096, NULL, 2, NULL);
  xTaskCreate(&Task_display, "Task display", 4096, NULL, 1, NULL);
}

void loop() {}