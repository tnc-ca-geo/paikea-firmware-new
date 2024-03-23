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
#include "pindefs.h"
#include "helpers.h"
#include "state.h"

// debug flags
#define DEBUG 1
#define GPS_ON true
#define SLEEP 1
#define USE_DISPLAY 1

/*
 * This variables will be persisted during deep sleep using slow RTC memory
 * but cleared upon reset.
 *
 * Leave in global scope because of:
 *
 * https://arduino.stackexchange.com/questions/72537/cant-create-a-rtc-data-attr-var-inside-a-class
 *
 * Tried to move to state.h but causes double import problems I don't
 * understand.
 */
RTC_DATA_ATTR time_t rtc_start = 0;
RTC_DATA_ATTR time_t rtc_expected_wakeup = 0;
RTC_DATA_ATTR time_t rtc_prior_uptime = 0;
RTC_DATA_ATTR bool rtc_first_fix = true;
RTC_DATA_ATTR bool rtc_first_run = true;

// Use to protect I2C shared between display and expander.
static SemaphoreHandle_t mutex_i2c;
// create TaskhHandles
static TaskHandle_t blinkTaskHandle = NULL;
static TaskHandle_t displayTaskHandle = NULL;
static TaskHandle_t gpsTaskHandle = NULL;
static TaskHandle_t timeSyncTaskHandle = NULL;
static TaskHandle_t storeUpTimeTaskHandle = NULL;
static TaskHandle_t waitForSleepTaskHandle = NULL;
static TaskHandle_t rockblockTaskHandle = NULL;

/*
 * Use Preferences for persistent data storage and debugging when fully powered
 * off. The Preferences library supposedly manages limited number of write
 * write cycles to this kind of storage.
 */
Preferences preferences;
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
SystemState state = SystemState();

/*
 * Blink LED 10. This is a useful indicator for the system running. A mutex is
 * needed since the I2C bus is shared with display and I/O expander.
 *
 * TODO: Replace with a more sophisticated way of system state indication.
 */
void Task_blink(void *pvParamaters) {
  bool blink_state = HIGH;
  for(;;) {
    // blink state seems to be reversed; deregister when LED is off
    if (state.getGoToSleep() && blink_state) {
      state.setBlinkSleepReady(true);
      vTaskDelete( blinkTaskHandle );
    } else {
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.digitalWrite(10, blink_state);
        xSemaphoreGive(mutex_i2c);
      } else Serial.println("Missed blink!");
      blink_state = !blink_state;
    }
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}

/*
 * Synchronize system time with GPS and re-establish "real-time" after sleep.
 * TODO: Some of this could be added to the state class
 */
void Task_time_sync(void *pvParameters) {
  time_t sync_time = 0;
  time_t real_time = 0;
  time_t uptime = 0;
  bool got_fix = false;
  for(;;) {
    real_time = state.getRealTime();
    if ( gps.updated ) {
#if DEBUG
      char bfr[255] = {0};
      sprintf(bfr, "GPS %d, %d\n", gps.epoch, gps.updated);
      // Serial.print(bfr);
# endif
      sync_time = ( esp_timer_get_time() - gps.gps_read_system_time )/1E6;
      real_time = gps.epoch + sync_time;
      if (rtc_first_fix) {
        rtc_start = real_time - uptime;
        rtc_first_fix = false;
      }
    } else real_time = rtc_expected_wakeup + esp_timer_get_time()/1E6;
    uptime = real_time - rtc_start;
    // write back values to state
    state.setRealTime( real_time );
    state.setUptime( uptime );
    // TODO: Move to GPS task.
    state.setGpsGotFix( gps.updated );
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
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
    if ( state.getDisplayOff() ) {
      Serial.println("Turn display off.");
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.off();
        xSemaphoreGive(mutex_i2c);
        vTaskDelete( displayTaskHandle );
      }
    } else {
      time_t time = state.getRealTime();
      state.getMessage(message_string);
      strftime(time_string, 22, "%F\n  %T", gmtime( &time ));
      snprintf(
        out_string, 50, "%s\nrssi %5d\n%s", time_string, state.getRssi(),
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
 *
 * TODO: for the Glue device we need a more sophisticated strategy of joins and
 * re-joins.
 */
void Task_rockblock(void *pvParameters) {
  char message[255] = {0};
  if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
    rockblock.toggle(true);
    xSemaphoreGive(mutex_i2c);
  }
  // rockblock.configure();
  // rockblock.join();
  for (;;) {
    rockblock.loop();
    rockblock.getLastMessage(message);
    state.setMessage(message);
    state.setRssi( rockblock.getRssi() );
    if (rockblock.getSendSuccess()) {
      rockblock.toggle( false );
      state.setRockblockSleepReady( true );
      vTaskDelete( rockblockTaskHandle );
    }
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}


// run GPS
void Task_gps(void *pvParameters) {
   char bfr[32] = {};
   for(;;) {
    // waiting for a GPS fix before going to sleep
    // TODO: this should move to the logic that triggers sleep state
    if (state.getGpsGotFix() || esp_timer_get_time()/1E6 > GPS_TIME_OUT) {
      Serial.println("GPS going to sleep");
      // disable GPS
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        gps.toggle(false);
        xSemaphoreGive(mutex_i2c);
        state.setGpsDone(true);
        vTaskDelete( gpsTaskHandle );
      }
    } else {
      gps.loop();
      if (gps.updated) {
        sprintf(bfr, "GPS updated: %.5f, %.5f\n", gps.lat, gps.lng);
        Serial.print(bfr);
      } else Serial.print(".");
    }
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}

/*
 * Waiting for all peripherials to be in the sleep ready state.
 */
void Task_wait_for_sleep(void *pvParameters) {
  esp_sleep_enable_timer_wakeup( SLEEP_TIME * uS_TO_S_FACTOR );
  char bfr[255];
  for (;;) {
    snprintf(bfr, 255,
      "Sleep ready state %d: blink %d, gps %d, rockblock %d\n",
      state.getGoToSleep(), state.getBlinkSleepReady(),
      state.getGpsDone(), state.getRockblockSleepReady() );
    Serial.print(bfr);
    // check whether to initiate sleep
    if (
      state.getBlinkSleepReady() &&
      state.getGpsDone() &&
      state.getRockblockSleepReady()
      ) {
      Serial.println("Going to sleep");
      // Sets expander to init state which uses hopefully the least power. All
      // pins configured as inputs. TODO: consultate datasheet to confirm
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.init();
        xSemaphoreGive(mutex_i2c);
      }
      // Print next send time to serial.
      snprintf(bfr, 255, "Next send time %d -> %d\n", state.getRealTime(),
        next_send_time(state.getRealTime(), 300));
      // Set expected wake up time and turn display off
      // Set values that need to survive sleep but NOT a cold start
      rtc_expected_wakeup = state.getRealTime() + SLEEP_TIME;
      state.setDisplayOff( true );
      // Give some time to turn display off
      // TODO: check actual state
      vTaskDelay( pdMS_TO_TICKS(300) );
      // Store state values into persistent storage
      state.persist();
      // Give some extra time for everything to settle down.
      vTaskDelay( pdMS_TO_TICKS( 250 ) );
      esp_deep_sleep_start();
    }
    // Check for sleep once a second.
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
  }
}

void Task_schedule(void *pvParameters) {
  bool gps_done = false;
  for (;;) {
    gps_done = state.getGpsDone();
    if ( rockblock.getSendSuccess() )  {
      if (xSemaphoreTake(mutex_i2c, 50) == pdTRUE) {
        rockblock.toggle(false);
        xSemaphoreGive(mutex_i2c);
      }
      state.setGoToSleep(true);
    }
    if ( gps_done == true && rockblock.getEnabled() ) {
      rockblock.queueMessage((char*) "Hello world!\0");
    }
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
  }
}


/*
 * This task generates a regular message and will be not used in normal
 * operation.
 */
void Task_send_message(void *pvParameters) {
  for (;;) {
    if (rockblock.queueMessage((char*) "Hello world!\0")) {
      Serial.println("\nSending message");
    } else Serial.println("Rockblock busy");
    vTaskDelay( pdMS_TO_TICKS( 30 * 1000 ) );
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
  // ---- Load state
  state.init();
  // ---- Give some time to stabilize -------------
  vTaskDelay( pdTICKS_TO_MS(100) );
  // ----- Init Display
  display.begin();
  // ----- Init Expander --------------------------
  expander.begin(PORT_EXPANDER_I2C_ADDRESS);
  // ----- Init Blink -----------------------------
  expander.pinMode(10, EXPANDER_OUTPUT);
  // ----- Turn on or off GPS ---------------------
  gps.toggle(GPS_ON);
  // -------------------------------

  /*
   * Load persisted values into RTC memory
   *
   * TODO: I would rather handle that in state.h
   */
  if ( rtc_first_run ) {
    preferences.begin("debug", false);
    rtc_prior_uptime = preferences.getUInt("uptime", 0);
    preferences.end();
    rtc_first_run = false;
    Serial.print("Last uptime: "); Serial.println(rtc_prior_uptime);
  }

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
  xTaskCreate(&Task_rockblock, "Task Rockblock", 4096, NULL, 10,
    &rockblockTaskHandle);
  xTaskCreate(&Task_wait_for_sleep, "Task wait for sleep", 4096, NULL, 13,
    NULL);
}

// Nothing to do here, thanks to FreeRTOS.
void loop() {}