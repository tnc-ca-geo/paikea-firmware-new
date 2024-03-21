// libraries
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
// my libraries
#include <tca95xx.h>
#include <gps.h>
#include <display.h>
#include <loraRockblock.h>
// project
#include "helpers.h"

// debug flags
#define DEBUG 1
#define GPS_ON true
#define SLEEP 1
#define USE_DISPLAY 1

// settings and pindefs
#define PORT_EXPANDER_I2C_ADDRESS 0x24
#define SHARED_SERIAL_RX_PIN 35
#define SHARED_SERIAL_TX_PIN 12
#define ROCKBLOCK_SERIAL_RX_PIN 34
#define ROCKBLOCK_SERIAL_TX_PIN 25
#define ROCKBLOCK_SERIAL_SPEED 115200
// 19200
#define NUM_TIMERS 1
#define uS_TO_S_FACTOR 1000000
#define SLEEP_TIME 30
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


// see https://www.ti.com/lit/ds/symlink/tca9535.pdf
uint8_t config_values[] = {
  0b00000000,  // Input register level port 0 (read only)
  0b00000000,  // Input register level port 1 (read only)
  // write output to those registers
  0b00000000,  // Output register level port 0
  0b00000000,  // Output register level port 1
  0b00000000,  // Pin polarity port 0
  0b00000000,  // Pin polarity port 1
  0b00111100,  // IO direction 1 ... input, 0 ... output port 0
  0b01110000   // IO direction 1 ... input, 0 ... output port 1
};

// Use to protect I2C
static SemaphoreHandle_t mutex_i2c;
// Use to protect the state object
static SemaphoreHandle_t mutex_state;
// Use to protect shared serial
static SemaphoreHandle_t mutex_serial;
// create TimerHandles
static TimerHandle_t xTimers[ NUM_TIMERS ];
// create TaskhHandles
static TaskHandle_t blinkTaskHandle = NULL;
static TaskHandle_t displayTaskHandle = NULL;
static TaskHandle_t gpsTaskHandle = NULL;
static TaskHandle_t timeSyncTaskHandle = NULL;
static TaskHandle_t storeUpTimeTaskHandle = NULL;
static TaskHandle_t waitForSleepTaskHandle = NULL;
static TaskHandle_t rockblockTaskHandle = NULL;

// Use preferences for debugging
// TODO: remove for production
Preferences preferences;

// Initialize Hardware
HardwareSerial shared_serial(1);
HardwareSerial rockblock_serial(2);
// Port Expander using i2c
Expander expander = Expander(Wire);
// GPS using UART
Gps gps = Gps(expander, shared_serial);
// Display using i2c, for development only.
LilyGoDisplay display = LilyGoDisplay(Wire);
// Rockblock or Lora Simulation
LoraRockblock rockblock = LoraRockblock(expander, rockblock_serial);

/* We need/use three different levels of variable persistence:
 *
 * 1. Memory that only persists when up between two sleep cycles.
 * 2. Memory that persists through sleep cycles (RTC).
 * 3. Storage that persists even if device is off (Preferences).
 */

// 1. Define state object
struct state {
  // Keep different timers for debugging
  time_t real_time;
  time_t prior_uptime;
  time_t uptime;
  // We inform all components to go to sleep gracefully and check whether
  // they are ready
  bool go_to_sleep;
  bool gps_sleep_ready;
  bool gps_got_fix;
  bool blink_sleep_ready;
  bool expander_sleep_ready;
  bool display_sleep_ready;
  bool rockblock_sleep_ready;
  int16_t rssi;
  char message[255] = {0};
} state;
// 2. State variables that should survive deep sleep to be stored in RTC memory
RTC_DATA_ATTR time_t rtc_start = 0;
RTC_DATA_ATTR time_t rtc_expected_wakeup = 0;
RTC_DATA_ATTR time_t rtc_prior_uptime;
RTC_DATA_ATTR bool rtc_first_fix = true;
RTC_DATA_ATTR bool rtc_first_run = true;
// 3. We use preferences to store any value that should be persisted while
// device is off.


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
  time_t sync_time = 0;
  time_t real_time = 0;
  time_t uptime = 0;
  bool got_fix = false;
  for(;;) {
    // read state to local variables to not hold on to mutex for too long
    if (xSemaphoreTake(mutex_state, 50) == pdTRUE) {
      real_time = state.real_time;
      xSemaphoreGive(mutex_state);
    }
    if (gps.updated) {

#if DEBUG
      char bfr[255] = {0};
      sprintf(bfr, "GPS %d, %d\n", gps.epoch, gps.updated);
      Serial.print(bfr);
# endif

      sync_time = (esp_timer_get_time() - gps.gps_read_system_time)/1E6;
      real_time = gps.epoch + sync_time;
      if (rtc_first_fix) {
        rtc_start = real_time - uptime;
        rtc_first_fix = false;
      }
      got_fix = true;
    } else {
      real_time = rtc_expected_wakeup + esp_timer_get_time()/1E6;
    }
    uptime = real_time - rtc_start;
    preferences.begin("debug", false);
    if (uptime - preferences.getUInt("uptime") > 60) {
      preferences.putUInt("uptime", uptime);
    }
    preferences.end();
    // write values back
    if (xSemaphoreTake(mutex_state, 50) == pdTRUE) {
      state.real_time = real_time;
      state.uptime = uptime;
      state.gps_got_fix = got_fix;
      xSemaphoreGive(mutex_state);
    }
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
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
        snprintf(
          out_string, 50, "%s\nrssi %5d\n%s", time_string, state.rssi,
          state.message);
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


void Task_rockblock(void *pvParameters) {
  if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
    rockblock.toggle(true);
    xSemaphoreGive(mutex_i2c);
  }
  // vTaskDelay( pdMS_TO_TICKS( 200 ) );
  // rockblock.configure();
  // rockblock.join();
  for (;;) {
    rockblock.loop();
    if (xSemaphoreTake(mutex_state, 200) == pdTRUE) {
      state.rssi = rockblock.getRssi();
      rockblock.getLastMessage(state.message);
      xSemaphoreGive(mutex_state);
    }
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}


// run GPS
void Task_gps(void *pvParameters) {
   char bfr[32] = {};
   for(;;) {
    // waiting for a GPS fix before going to sleep
    // TODO: add timeout if we never get one
    // TODO: this should move to the logic that triggers sleep state
    if ((state.gps_got_fix) || esp_timer_get_time()/1E6 > GPS_TIME_OUT) {
      Serial.println("GPS going to sleep");
      // disable GPS
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        gps.toggle(false);
        // rockblock.toggle(true);
        state.gps_sleep_ready = true;
        // Serial.println("Start Rockblock");
        xSemaphoreGive(mutex_i2c);
        // TODO: Move somewhere else!
        // create the Rockblock task
        // xTaskCreate(
        //  &Task_rockblock, "Task Rockblock", 4096, NULL, 6,
        //  &rockblockTaskHandle);
        vTaskDelete( gpsTaskHandle );
      }
    } else {
      if (xSemaphoreTake(mutex_serial, 200) == pdTRUE) {
        gps.loop();
        xSemaphoreGive(mutex_serial);
      } else {
        Serial.println("Could not obtain MUTEX");
      }
      if (gps.updated) {
        sprintf(bfr, "GPS updated: %.5f, %.5f\n", gps.lat, gps.lng);
        Serial.print(bfr);
      } else {
        Serial.println("Waiting for GPS");
      }
    }
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}


/*
 * Waiting for all peripherials to be in a sleep ready state
 */
void Task_wait_for_sleep(void *pvParameters) {
  esp_sleep_enable_timer_wakeup( SLEEP_TIME * uS_TO_S_FACTOR );
  char bfr[255];
  for (;;) {
    sprintf(
      bfr, "Sleep ready state %d: blink %d, display %d, gps %d, rockblock %d\n",
      state.go_to_sleep, state.blink_sleep_ready, state.display_sleep_ready,
      state.gps_sleep_ready, state.rockblock_sleep_ready);
    Serial.print(bfr);
    bool sleep_ready = state.blink_sleep_ready && state.display_sleep_ready &&
      state.gps_sleep_ready && state.rockblock_sleep_ready;
    if (sleep_ready) {
      Serial.println("Going to sleep");
      // sets all expander pins to input to conserve power
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.init();
        xSemaphoreGive(mutex_i2c);
      }
      // print next send time
      sprintf(
        bfr, "Next send time %d -> %d\n",
        state.real_time, next_send_time(state.real_time, 300));
      Serial.print(bfr);
      // set expected wake up time
      if (xSemaphoreTake(mutex_state, 50) == pdTRUE) {
        rtc_expected_wakeup = state.real_time + SLEEP_TIME;
        xSemaphoreGive(mutex_state);
      }
      esp_deep_sleep_start();
    }
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
  }
}

void Task_schedule(void *pvParameters) {
  for (;;) {
    if ( state.gps_sleep_ready ) {
      if ( !state.rockblock_sleep_ready && !rockblockTaskHandle ) {
        xTaskCreate(&Task_rockblock, "Task Rockblock", 4096, NULL, 10,
          &rockblockTaskHandle);
      } else {
        state.go_to_sleep = true;
        if (!waitForSleepTaskHandle) {
          xTaskCreate(&Task_wait_for_sleep, "Check Sleep ready", 2048, NULL, 10,
            &waitForSleepTaskHandle);
        }
      }
    }
    vTaskDelay( pdMS_TO_TICKS( 500 ) );
  }
}

void Task_send_message(void *pvParameters) {
  for (;;) {
    if (rockblock.queueMessage((char*) "Hello world!\0")) {
      Serial.println("\nSending message");
    } else {
      Serial.println("Rockblock busy");
    };
    vTaskDelay( pdMS_TO_TICKS( 30 * 1000 ) );
  }
}


void setup() {
  // ---- Start Serial for debugging --------------
  Serial.begin(115200);
  // ----- Init Shared Serial ---------------------
  shared_serial.begin(
    9600, SERIAL_8N1, SHARED_SERIAL_RX_PIN, SHARED_SERIAL_TX_PIN);
  rockblock_serial.begin(
    ROCKBLOCK_SERIAL_SPEED, SERIAL_8N1, ROCKBLOCK_SERIAL_RX_PIN,
    ROCKBLOCK_SERIAL_TX_PIN);
  // ---- Start I2C bus for peripherials ----------
  Wire.begin();
  // ---- Give some time to stabilize -------------
  delay(100);
  // ----------------------------------------------
  // ----- Init Display
  display.begin();
  // ----- Init Expander --------------------------
  expander.begin(PORT_EXPANDER_I2C_ADDRESS);
  // ----- Init Blink -----------------------------
  expander.pinMode(10, EXPANDER_OUTPUT);
  // ----- Turn on or off GPS ---------------------
  gps.toggle(GPS_ON);
  // -------------------------------
  // ---- Load persisted values into state --------
  if ( rtc_first_run ) {
    preferences.begin("debug", false);
    rtc_prior_uptime = preferences.getUInt("uptime", 0);
    preferences.end();
    rtc_first_run = false;
  }

  // ------ FreeRTOS setup ------------------------
  // a mutex managing the use of the shared I2C bus
  mutex_i2c = xSemaphoreCreateMutex();
  // a mutex protecting shared Serial
  mutex_serial = xSemaphoreCreateMutex();
  // a mutex managing access to the state object
  mutex_state = xSemaphoreCreateMutex();

  // create simple tasks (for now)
  xTaskCreate(&Task_blink, "Task blink", 4096, NULL, 3, &blinkTaskHandle);
  // xTaskCreate(&Task_gps, "Task gps", 4096, NULL, 12, &gpsTaskHandle);
  xTaskCreate(&Task_time_sync, "Task time sync", 4096, NULL, 10,
    &timeSyncTaskHandle);
  xTaskCreate(&Task_display, "Task display", 4096, NULL, 9, &displayTaskHandle);
  // xTaskCreate(&Task_schedule, "Task schedule", 4096, NULL, 10, NULL);
  xTaskCreate(&Task_rockblock, "Task Rockblock", 4096, NULL, 10,
    &rockblockTaskHandle);
  xTaskCreate(&Task_send_message, "Task send message", 4096, NULL, 10, NULL);

  // create timers
  // xTimers[0] = xTimerCreate(
  //  "A timer", pdMS_TO_TICKS( 10 * 1000 ), pdTRUE, 0, vTimerCallback);
  // xTimerStart(xTimers[0], 0);
  // ------------------------------------------

}


void loop() {}