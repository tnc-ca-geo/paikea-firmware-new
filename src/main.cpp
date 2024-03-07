// libraries
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
// my libraries
#include <tca95xx.h>
#include <gps.h>
#include <display.h>
// project
#include "helpers.h"


#define PORT_EXPANDER_I2C_ADDRESS 0x24
#define GPS_RX_PIN 35
#define GPS_TX_PIN 12
#define NUM_TIMERS 1
#define uS_TO_S_FACTOR 1000000

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
static SemaphoreHandle_t mutex_i2c;
// Use to protect the state object
static SemaphoreHandle_t mutex_state;
// create TimerHandles
static TimerHandle_t xTimers[ NUM_TIMERS ];
// create TaskhHandles
static TaskHandle_t blinkTaskHandle = NULL;
static TaskHandle_t displayTaskHandle = NULL;
static TaskHandle_t gpsTaskHandle = NULL;
static TaskHandle_t timeSyncTaskHandle = NULL;
static TaskHandle_t storeUpTimeTaskHandle = NULL;
static TaskHandle_t waitForSleepTaskHandle = NULL;

// Use preferences for debugging
// TODO: remove for production
Preferences preferences;

// Initialize Hardware
// Port Expander using i2c
Expander expander = Expander(Wire);
// GPS using UART
Gps gps = Gps();
// Display using i2c, for development only.
LilyGoDisplay display = LilyGoDisplay(Wire);

// store state
struct state {
  time_t real_time;
  time_t prior_uptime;
  // we inform all components to go to sleep gracefully and then check whether
  // they are ready
  bool go_to_sleep;
  bool gps_sleep_ready;
  bool blink_sleep_ready;
  bool expander_sleep_ready;
  bool display_sleep_ready;
} state;
// state variables that should survive deep sleep, store in RTC memory
RTC_DATA_ATTR int uptime = 0;

void Task_store_uptime(void *pvParameters) {
    for (;;) {
      preferences.begin("debug", false);
      preferences.putUInt("uptime", esp_timer_get_time()/1000000);
      preferences.end();
      // We really need to do this only once a minute to make sure to not to
      // write to often to EEPROM
      vTaskDelay( pdMS_TO_TICKS( 60000 ) );
    }
}

/*
 * Blink LED 10. This is a useful indicator for the system running. We need
 * a mutex here since we are using the I2C bus shared with the display.
 */
void Task_blink(void *pvParamaters) {
  bool blink_state = HIGH;
  for(;;) {
    // blink state seems to be reversed; make sure we deregister when LED off
    if (state.go_to_sleep && blink_state) {
      state.blink_sleep_ready = true;
      // deregister blink task
      vTaskDelete( blinkTaskHandle );
    } else {
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.digitalWrite(10, blink_state);
        xSemaphoreGive(mutex_i2c);
      } else {
        Serial.println("missed blink");
      }
      blink_state = !blink_state;
    }
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }

}

/*
 * This task synchronizes the system time with the GPS and re-establishes real
 * time after sleep.
 */
void Task_time_sync(void *pvParameters) {
  uint32_t sync_time;
  for(;;) {
    sync_time = (esp_timer_get_time() - gps.gps_read_system_time)/1000;
    if (xSemaphoreTake(mutex_state, 200) == pdTRUE) {
      // sync_time is typically below a second, but I am putting this hear
      // just in case it gets busy
      state.real_time = gps.epoch + sync_time/1000;
      uptime = esp_timer_get_time()/1000000;
    xSemaphoreGive(mutex_state);
    }
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
  }
}

// run GPS
void Task_gps(void *pvParameters) {
   for(;;) {
    if (state.go_to_sleep) {
      // disable GPS
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.digitalWrite(0, LOW);
        state.gps_sleep_ready = true;
        xSemaphoreGive(mutex_i2c);
        vTaskDelete( gpsTaskHandle );
      }
    } else {
      gps.loop();
      if (gps.valid) {
        Serial.println("GPS is valid!");
      } else {
        Serial.println("Waiting for GPS");
      }
    }
    vTaskDelay( pdMS_TO_TICKS( 500 ) );
  }
}

/*
 * Output part of the state to the display. We are using a mutex since we are
 * using the I2C bus.
 */
void Task_display(void *pvParameters) {
  uint8_t size = 0;
  char out_string[255] = {0};
  char time_string[20] = {0};
  for(;;) {
    if (state.go_to_sleep) {
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.off();
        state.display_sleep_ready = true;
        xSemaphoreGive(mutex_i2c);
        vTaskDelete( displayTaskHandle );
      }
    } else {
      if (xSemaphoreTake(mutex_state, 200) == pdTRUE) {
        strftime(time_string, 22, "%F\n  %T", gmtime( &state.real_time ));
        sprintf(
          out_string, "%s\nup %7d\nlst %6d", time_string, uptime,
          state.prior_uptime);
        xSemaphoreGive(mutex_state);
      }
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.set(out_string);
        xSemaphoreGive(mutex_i2c);
      }
    }
    vTaskDelay( pdMS_TO_TICKS( 50 ) );
  }
}

/*
 * Waiting for all peripherials to be in a sleep ready state
 */
void Task_wait_for_sleep(void *pvParameters) {
  esp_sleep_enable_timer_wakeup(60 * uS_TO_S_FACTOR);
  char bfr[255];
  for (;;) {
    sprintf(
      bfr, "Sleep ready state %d: blink %d, display %d, gps %d\n",
      state.go_to_sleep, state.blink_sleep_ready, state.display_sleep_ready,
      state.gps_sleep_ready);
    Serial.print(bfr);
    bool sleep_ready = state.blink_sleep_ready && state.display_sleep_ready &&
      state.gps_sleep_ready;
    if (sleep_ready) {
      Serial.println("Going to sleep");
      // sets all expander pins to input to conserve power
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.init();
        xSemaphoreGive(mutex_i2c);
      }
      esp_deep_sleep_start();
    }
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
  }
}

/*
 * Since our real workflow (taking a certain time) is not implemented yet
 * we are using a timer to go to deep sleep.
 */
void vTimerCallback(TimerHandle_t xtimer) {
    Serial.println("Start sleep sequence");
    state.go_to_sleep = true;
    if (!waitForSleepTaskHandle) {
      xTaskCreate(&Task_wait_for_sleep, "Check Sleep ready", 2048, NULL, 10,
        &waitForSleepTaskHandle);
    }
    // this is not part of freeRTOS so we need to make sure everything is safe
    // before we go here
    // esp_sleep_enable_timer_wakeup(60 * uS_TO_S_FACTOR);
    // esp_deep_sleep_start();
  }

void setup() {
  // Serial is shared
  // TODO: wrap it in some debug code
  Serial.begin(115200);
  Wire.begin();
  // this is for debugging.
  preferences.begin("debug", false);
  // load the last up time into state
  state.prior_uptime = preferences.getUInt("uptime", 0);
  preferences.end();
  // a mutex managing the use of the shared I2C bus
  mutex_i2c = xSemaphoreCreateMutex();
  // a mutex managing access to the state object
  mutex_state = xSemaphoreCreateMutex();
  // TODO: move address to library since we don't really have a choice here
  // Init display and expander, both relay on Wire.
  expander.begin(PORT_EXPANDER_I2C_ADDRESS);
  display.begin();
  // Init GPS
  gps.begin(GPS_RX_PIN, GPS_TX_PIN);
  // Arduino keywords INPUT and OUTPUT don't work here since they associate
  // different values
  // expander.pinMode(3, EXPANDER_INPUT);
  // configure blink
  expander.pinMode(10, EXPANDER_OUTPUT);
  // enable GPS
  expander.pinMode(0, EXPANDER_OUTPUT);
  expander.digitalWrite(0, HIGH);

  // create simple tasks (for now)
  xTaskCreate(&Task_blink, "Task blink", 4096, NULL, 3, &blinkTaskHandle);
  xTaskCreate(&Task_gps, "Task gps", 4096, NULL, 5, &gpsTaskHandle);
  xTaskCreate(&Task_time_sync, "Task time sync", 4096, NULL, 10,
    &timeSyncTaskHandle);
  xTaskCreate(&Task_display, "Task display", 4096, NULL, 9, &displayTaskHandle);
  xTaskCreate(&Task_store_uptime, "Task store up time", 4096, NULL, 1,
    &storeUpTimeTaskHandle);
  // create timer
  xTimers[0] = xTimerCreate(
    "A timer", pdMS_TO_TICKS( 10000 ), pdTRUE, 0, vTimerCallback);
  xTimerStart(xTimers[0], 0);
}

void loop() {}