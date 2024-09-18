// libraries
#include <Arduino.h>
#include <Wire.h>
// my libraries
#include <tca95xx.h>
#include <gps.h>
#include <display.h>
#include <loraRockblock.h>
#include <stateType.h>
#include <scoutMessages.h>
#include <storage.h>
// project
#include "pindefs.h"

// debug flags
#define DEBUG 1
#define SLEEP 1
#define USE_DISPLAY 1

// Use to protect I2C shared between display and expander.
static SemaphoreHandle_t mutex_i2c;
// Use to protect state object if not atomic, ideally we would modify state
// object only in one place
static SemaphoreHandle_t mutex_state;
// create TaskhHandles, reduce number of tasks to one for each peripherial and
// one for the main state object
static TaskHandle_t blinkTaskHandle = NULL;
static TaskHandle_t displayTaskHandle = NULL;
static TaskHandle_t gpsTaskHandle = NULL;
static TaskHandle_t timeSyncTaskHandle = NULL;
static TaskHandle_t storeUpTimeTaskHandle = NULL;
static TaskHandle_t waitForSleepTaskHandle = NULL;
static TaskHandle_t rockblockTaskHandle = NULL;

// Initialize Hardware
HardwareSerial gps_serial(1);
HardwareSerial rockblock_serial(2);
// Port Expander using i2c
Expander expander = Expander(Wire);
// GPS using UART
Gps gps = Gps(expander, gps_serial);
// Display using i2c, for development only.
LilyGoDisplay display = LilyGoDisplay(Wire);
// Rockblock or Lora Simulation
LoraRockblock rockblock = LoraRockblock(expander, rockblock_serial);
// State object
systemState state;
// Storage
ScoutStorage storage = ScoutStorage();
// Messages
ScoutMessages scout_messages = ScoutMessages();


void setTime(time_t time) {
  if (xSemaphoreTake(mutex_state, 200) == pdTRUE) {
    state.real_time = time;
    state.time_read_system_time = esp_timer_get_time()/1E6;
    if (state.first_fix) {
      state.start_time = state.real_time - state.time_read_system_time;
    }
    xSemaphoreGive(mutex_state);
  }
}

time_t getNextWakeupTime(time_t now, uint16_t delay) {
  time_t full_hour = int(now/3600) * 3600;
  uint16_t cycles = int(now % 3600/delay);
  return full_hour + (cycles + 1) * delay;
}

void syncTime() {
  if (xSemaphoreTake(mutex_state, 200) == pdTRUE) {
    time_t set_time = esp_timer_get_time()/1E6;
    time_t time = state.real_time + set_time - state.time_read_system_time;
    state.real_time = time;
    state.time_read_system_time = set_time;
    state.expected_wakeup = getNextWakeupTime(time, state.frequency);
    state.uptime = state.real_time - state.start_time;
    xSemaphoreGive(mutex_state);
  }
}

/*
 * Blink LED 10. This is a useful indicator for the system running. A mutex is
 * needed since the I2C bus is shared with display and I/O expander.
 *
 * TODO: Replace with a more sophisticated way of system state indication.
 * Start from the current firmware behavior.
 */
void Task_blink(void *pvParamaters) {
  bool blink_state = HIGH;
  for(;;) {
    // deregister when LED is off
    if (state.go_to_sleep && blink_state) {
      state.blink_sleep_ready = true;
      vTaskDelete( blinkTaskHandle );
    } else {
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.digitalWrite(10, blink_state);
        xSemaphoreGive(mutex_i2c);
      }
      blink_state = !blink_state;
    }
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}

/*
 * Synchronize system time with GPS time and trigger regular time calculations.
 */
void Task_time_sync(void *pvParameters) {
  for(;;) {
    syncTime();
    // if ( gps.updated ) { state.setRealTime( gps.get_corrected_epoch(), true ); }
    // else state.sync();
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}

/*
 * Output part of the state to the display. We are using a mutex since we are
 * using the I2C bus.
 */
void Task_display(void *pvParameters) {
  uint8_t size = 0;
  char message_string[255] = {0};
  char out_string[255] = {0};
  char time_string[22] = {0};
  for(;;) {
    if ( state.display_off ) {
      Serial.println("Turn display off.");
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.off();
        xSemaphoreGive(mutex_i2c);
        vTaskDelete( displayTaskHandle );
      }
    } else {
      time_t time = state.real_time;
      strftime(time_string, 22, "%F\n  %T", gmtime( &state.real_time ));
      snprintf(
        out_string, 50, "%s\nrssi %5d\n%s", time_string, state.rssi,
        message_string);
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.set(out_string);
        xSemaphoreGive(mutex_i2c);
      }
    }
    vTaskDelay( pdMS_TO_TICKS( 50 ) );
  }
}

/*
 * Running the radio. For now we are using a LoraWAN radio as stand-in for
 * Rockblock.
 */
void Task_rockblock(void *pvParameters) {
  Serial.print("Start RB loop, ");
  Serial.print("rtc_first_run: "), Serial.println(rtc_first_run);
  if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
    rockblock.toggle(true);
    xSemaphoreGive(mutex_i2c);
  }
  if (rtc_first_run) {
    Serial.println("Configure LoraWAN");
    rockblock.configure();
    // Re-joining every single time for now. More complex joining procedures
    // should be implemented when we really use LoRaWAN, see e.g.
    // https://www.thethingsnetwork.org/forum/t/how-to-handle-an-automatic-re-join-process/34183/16
    rockblock.beginJoin();
  }
  for (;;) {
    rockblock.loop();
    vTaskDelay( pdMS_TO_TICKS( 500 ) );
  }
}

/*
 * Run GPS. Disable GPS and remove task when we got a fix.
 */
void Task_gps(void *pvParameters) {
  char bfr[32] = {};
  if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
    gps.enable();
    xSemaphoreGive(mutex_i2c);
  }
  for(;;) {
    gps.loop();
    if (gps.updated) {
      sprintf(bfr, "GPS updated: %.05f, %.05f\n", gps.lat, gps.lng);
      state.lat = gps.lat;
      state.lng = gps.lng;
      state.gps_done = true;
      setTime( gps.get_corrected_epoch() );
      // state.setLatitude(gps.lat);
      // tate.setLongitude(gps.lng);
      // state.setGpsDone(true);
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        gps.disable();
        xSemaphoreGive(mutex_i2c);
      }
      vTaskDelete( gpsTaskHandle );
    }
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}

/*
 * Waiting for all peripherials to be in the sleep ready state.
 */
void Task_wait_for_sleep(void *pvParameters) {
  char bfr[255];
  for (;;) {
    // Check for sleep once a second.
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    // Check whether system is ready to go to sleep, and persist values
    if (state.gps_done && state.blink_sleep_ready && state.rockblock_done) {
      Serial.println("Going to sleep");
      // Sets expander to init state to conserve power.
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.init();
        xSemaphoreGive(mutex_i2c);
      }
      // Give some time to turn display off
      storage.store( state );
      // TODO: check actual state
      state.display_off = true;
      vTaskDelay( pdMS_TO_TICKS(300) );
      uint32_t difference = state.expected_wakeup - state.real_time;
      snprintf(
        bfr, 255, "Frequency: %d, next send time: %d, difference: %d\n",
        (uint32_t) state.frequency, (uint32_t) state.expected_wakeup,
        difference);
      Serial.print(bfr);
      esp_sleep_enable_timer_wakeup( difference * 1E6 );
      esp_deep_sleep_start();
    }
  }
}

/*
 * Task regularly sending messages (for radio only testing).
 */
void Task_message(void *pvParameters) {
  for (;;) {
    if ( rockblock.available() ) rockblock.sendMessage((char*) "Hello world!\0");
    vTaskDelay( pdMS_TO_TICKS( 10000 ) );
  }
}


/*
 * Task - Manage timing and logic of all components.
 */
void Task_schedule(void *pvParameters) {
  char message[255] = {0};
  for (;;) {

    vTaskDelay( pdMS_TO_TICKS( 1000 ) );

    if ( gps.updated && !state.message_sent ) {
      scout_messages.createPK001(state, message);
      rockblock.sendMessage(message);
      state.message_sent = true;
    }

    if ( state.message_sent && rockblock.getSendSuccess() ) {
      state.rockblock_done = true;
    }

    if ( gps.updated && state.message_sent ) {
      // We are setting a flag informing all components to go into sleep state.
      // Sleep will wait until everyone is ready.
      state.go_to_sleep = true;
    }

  }
}


void setup() {
  // ---- Start Serial for debugging --------------
  Serial.begin(115200);
  // ----- Init both HardwareSerial channels ---------------------
  gps_serial.begin(
    9600, SERIAL_8N1, GPS_SERIAL_RX_PIN, GPS_SERIAL_TX_PIN);
  rockblock_serial.begin(
    ROCKBLOCK_SERIAL_SPEED, SERIAL_8N1, ROCKBLOCK_SERIAL_RX_PIN,
    ROCKBLOCK_SERIAL_TX_PIN);
  // ---- Start I2C bus for peripherials ----------
  Wire.begin();
  // ---- Restore state
  storage.restore(state);
  // ----- Init Display
  display.begin();
  // ----- Init Expander --------------------------
  expander.begin(PORT_EXPANDER_I2C_ADDRESS);
  // ----- Init Blink -----------------------------
  expander.pinMode(10, EXPANDER_OUTPUT);
   // ---- Give some time to stabilize -------------
  vTaskDelay( pdTICKS_TO_MS(100) );

  /*
   * FreeRTOS setup
   */

  /*
   * Mutex managing shared I2C bus.
   *
   * TODO: Pass the mutex as reference to the classes using it (?)
   *
   * For now we have to manually take it whenever we call something using I2C:
   *
   *  - expander: blink and GPS enable/disable
   *  - display
   */
  mutex_i2c = xSemaphoreCreateMutex();
  mutex_state = xSemaphoreCreateMutex();

  /*
   * Create and start simple tasks (for now). All components are started to
   * grant initialization time before use.
   *
   * TODO: Sophisticate flow between tasks
   */
  xTaskCreate(&Task_blink, "Task blink", 4096, NULL, 3, &blinkTaskHandle);
  xTaskCreate(&Task_gps, "Task gps", 4096, NULL, 12, &gpsTaskHandle);
  xTaskCreate(&Task_time_sync, "Task time sync", 4096, NULL, 10,
    &timeSyncTaskHandle);
  xTaskCreate(&Task_display, "Task display", 4096, NULL, 9, &displayTaskHandle);
  xTaskCreate(&Task_schedule, "Task schedule", 4096, NULL, 10, NULL);
  // xTaskCreate(&Task_message, "Task message", 4096, NULL, 14, NULL);
  xTaskCreate(
    &Task_rockblock, "Task Rockblock", 4096, NULL, 10, &rockblockTaskHandle);
  // We are putting this in a task because at some point we need to stop
  // waiting for other tasks.
  xTaskCreate(&Task_wait_for_sleep, "Task wait for sleep", 4096, NULL, 13,
    NULL);
}


// Nothing to do here, thank you FreeRTOS.
void loop() {}