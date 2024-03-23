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

// Use to protect I2C
static SemaphoreHandle_t mutex_i2c;
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


// Use preferences for debugging and data storage when off
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
SystemState sys_state = SystemState();

/* We need/use three different levels of variable persistence:
 *
 * 1. Memory that only persists when up between two sleep cycles.
 * 2. Memory that persists through sleep cycles (RTC).
 * 3. Storage that persists even if device is off (Preferences).
 */


/*
 * 1. State variables that should survive deep sleep to be stored in RTC memory
 * TODO: Move to state.
 */
RTC_DATA_ATTR time_t rtc_start = 0;
RTC_DATA_ATTR time_t rtc_expected_wakeup = 0;
RTC_DATA_ATTR time_t rtc_prior_uptime = 0;
RTC_DATA_ATTR bool rtc_first_fix = true;
RTC_DATA_ATTR bool rtc_first_run = true;
/*
 * 3. We use preferences to store any value that should be persisted while
 * device is off.
 * TODO: for the Glue device we need to have a strategy managing the life time
 * of this form of storage with about 10.000 write cycles. According to the
 * documentation this is already handled by preferences and LittleFS
 */

/*
 * Blink LED 10. This is a useful indicator for the system running. A mutex
 * is needed since the I2C bus is shared with display and I/O expander.
 * TODO: Replace with a sophisticated way of system state indication.
 */
void Task_blink(void *pvParamaters) {
  bool blink_state = HIGH;
  for(;;) {
    // blink state seems to be reversed; deregister when LED is off
    if (sys_state.getGoToSleep() && blink_state) {
      sys_state.setBlinkSleepReady(true);
      vTaskDelete( blinkTaskHandle );
    } else {
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.digitalWrite(10, blink_state);
        xSemaphoreGive(mutex_i2c);
      } else Serial.println("missed blink");
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
    real_time = sys_state.getRealTime();
    if (gps.updated) {
#if DEBUG
      char bfr[255] = {0};
      sprintf(bfr, "GPS %d, %d\n", gps.epoch, gps.updated);
      // Serial.print(bfr);
# endif
      sync_time = (esp_timer_get_time() - gps.gps_read_system_time)/1E6;
      real_time = gps.epoch + sync_time;
      if (rtc_first_fix) {
        rtc_start = real_time - uptime;
        rtc_first_fix = false;
      }
    } else real_time = rtc_expected_wakeup + esp_timer_get_time()/1E6;
    uptime = real_time - rtc_start;
    // write back values state
    sys_state.setGpsGotFix( gps.updated );
    sys_state.setRealTime(real_time);
    sys_state.setUptime(uptime);
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
    if ( sys_state.getDisplayOff() ) {
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.off();
        xSemaphoreGive(mutex_i2c);
        vTaskDelete( displayTaskHandle );
      }
    } else {
      time_t time = sys_state.getRealTime();
      sys_state.getMessage(message_string);
      strftime(time_string, 22, "%F\n  %T", gmtime( &time ));
      snprintf(
        out_string, 50, "%s\nrssi %5d\n%s", time_string, sys_state.getRssi(),
        message_string);
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.set(out_string);
        xSemaphoreGive(mutex_i2c);
      }
    }
    vTaskDelay( pdMS_TO_TICKS( 50 ) );
  }
}


void Task_rockblock(void *pvParameters) {
  char message[255] = {0};
  if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
    rockblock.toggle(true);
    xSemaphoreGive(mutex_i2c);
  }
  // TODO: For the glue device we need to have a more sophisticated strategy
  // to initiate joins and rejoins. Here we have a registered device and use it
  // in place of a Rockblock. A rockblock does not need a join procedure.
  // rockblock.configure();
  // rockblock.join();
  for (;;) {
    rockblock.loop();
    rockblock.getLastMessage(message);
    sys_state.setMessage(message);
    sys_state.setRssi( rockblock.getRssi() );
    if (rockblock.getSendSuccess()) {
      rockblock.toggle( false );
      sys_state.setRockblockSleepReady( true );
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
    // TODO: add timeout if we never get one
    // TODO: this should move to the logic that triggers sleep state
    if (sys_state.getGpsGotFix() || esp_timer_get_time()/1E6 > GPS_TIME_OUT) {
      Serial.println("GPS going to sleep");
      // disable GPS
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        gps.toggle(false);
        xSemaphoreGive(mutex_i2c);
        sys_state.setGpsDone(true);
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
 * Helper function that puts state values into persistent storage
 */
void persistStateVariables() {
  Serial.println("Storing values");
  preferences.begin("debug", false);
  preferences.putUInt("uptime", sys_state.getUptime());
  preferences.end();
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
      sys_state.getGoToSleep(), sys_state.getBlinkSleepReady(),
      sys_state.getGpsDone(), sys_state.getRockblockSleepReady() );
    Serial.print(bfr);
    // check whether to initiate sleep
    if (
      sys_state.getBlinkSleepReady() &&
      sys_state.getGpsDone() &&
      sys_state.getRockblockSleepReady()
      ) {
      Serial.println("Going to sleep");
      // Sets expander to init state which uses hopefully the least power. All
      // pins configured as inputs. TODO: consultate datasheet to confirm
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.init();
        xSemaphoreGive(mutex_i2c);
      }
      // Print next send time to serial.
      snprintf(bfr, 255, "Next send time %d -> %d\n", sys_state.getRealTime(),
        next_send_time(sys_state.getRealTime(), 300));
      // Set expected wake up time and turn display off
      // Set values that need to survive sleep but NOT a cold start
      rtc_expected_wakeup = sys_state.getRealTime() + SLEEP_TIME;
      sys_state.setDisplayOff( true );
      // Store state values into persistent storage
      persistStateVariables();
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
    gps_done = sys_state.getGpsDone();
    if ( rockblock.getSendSuccess() )  {
      if (xSemaphoreTake(mutex_i2c, 50) == pdTRUE) {
        rockblock.toggle(false);
        xSemaphoreGive(mutex_i2c);
      }
      sys_state.setGoToSleep(true);
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
  sys_state.init();
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
   * ---- Load persisted values into state --------
   */

  if ( rtc_first_run ) {
    preferences.begin("debug", false);
    rtc_prior_uptime = preferences.getUInt("uptime", 0);
    preferences.end();
    rtc_first_run = false;
    Serial.print("Last uptime: "); Serial.println(rtc_prior_uptime);
  }

  // ------ FreeRTOS setup ------------------------
  // A mutex managing the use of the shared I2C bus
  mutex_i2c = xSemaphoreCreateMutex();

  /*
   * Create simple tasks (for now). We create all tasks here since some of the
   * hardware needs to boot as well.
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

// Since we are using RTOS nothing to do here.
void loop() {}