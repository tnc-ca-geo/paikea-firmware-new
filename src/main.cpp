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
// static TaskHandle_t blinkTaskHandle = NULL;
static TaskHandle_t displayTaskHandle = NULL;
static TaskHandle_t gpsTaskHandle = NULL;
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
ScoutMessages messages;

/*
 * Read system time and return UNIX epoch.
 */
time_t getTime() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return tv_now.tv_sec;
}

/*
 * Set system time. Set also corrected start time if start is not already set.
 * TODO: remove reference to state
 */
void setTime(time_t time) {
  struct timeval new_time;
  time_t old_time = getTime();
  new_time.tv_sec = time;
  settimeofday(&new_time, NULL);
  if (!state.start_time) { state.start_time = time - old_time; }
}

time_t getNextWakeupTime(time_t now, uint16_t delay) {
  time_t full_hour = int(now/3600) * 3600;
  uint16_t cycles = int(now % 3600/delay);
  return full_hour + (cycles + 1) * delay;
}

/*
 * Read the battery voltage (on start-up, no reason to get fancy here)
 */
float readBatteryVoltage() {
  float readings = 0;
  for (uint8_t i=0; i<10; i++) {
    readings += (float) analogReadMilliVolts(BATT_ADC)/10000;
  }
  return readings * (BATT_R_UPPER + BATT_R_LOWER)/BATT_R_LOWER;
}

/*
 * Blink LED 1 and 0 as simple system indicator.
 * A mutex is needed since the I2C bus is shared with display and I/O expander.
 * TODO: move mutex to tca95xx module?
 *
 * LED1 - Blink 1s before GPS fix
 * LED1 - Solid after GPS fix.
 * LED0 - Solid after radio transmitted.
 * OFF - Sleep (triggered by resetting the expander in the sleep function)
 */
void Task_blink(void *pvParamaters) {
  bool blink_state = HIGH;
  for(;;) {
    if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
      expander.digitalWrite( LED01, blink_state || state.gps_done );
      expander.digitalWrite( LED00, state.rockblock_done );
      xSemaphoreGive(mutex_i2c);
    }
    blink_state = !blink_state;
    vTaskDelay( pdMS_TO_TICKS( 200 ) );
  }
}

/*
 * DEBUG ONLY: Output part of the state to the display. Use mutex for I2C bus.
 */
void Task_display(void *pvParameters) {
  uint8_t size = 0;
  char message_string[44] = {0};
  char out_string[44] = {0};
  char time_string[22] = {0};
  time_t time = 0;
  state.display_off = false;
  for(;;) {
    if ( state.go_to_sleep ) {
      Serial.println("Turn display off.");
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.off();
        xSemaphoreGive(mutex_i2c);
        state.display_off = true;
        vTaskDelete( displayTaskHandle );
      }
    } else {
      time = getTime();
      strftime(time_string, 22, "%F\n  %T", gmtime( &time ));
      snprintf(
        out_string, 50, "%s\nrssi %5d\n%s", time_string, state.rssi,
        message_string);
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.set(out_string);
        xSemaphoreGive(mutex_i2c);
      }
    }vTaskDelay( pdMS_TO_TICKS( 200 ) );
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
  }
}

/*
 * Running the radio. For now we are using a LoraWAN radio as stand-in for
 * Rockblock.
 */
void Task_rockblock(void *pvParameters) {
  Serial.println("Start radio loop");
  if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
    rockblock.toggle(true);
    xSemaphoreGive(mutex_i2c);
  }
  if (rtc_first_run) {
    Serial.println("Configure LoraWAN");
    rockblock.configure();
    // Re-joining every single time for now. More complex joining procedures
    // should be implemented when we really use LoRaWAN, see e.g.
    // https://www.thethingsnetwork.org/forum/t/
    // how-to-handle-an-automatic-re-join-process/34183/16
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
      state.heading = gps.heading;
      state.speed = gps.speed;
      state.gps_done = true;
      state.gps_read_time = gps.get_corrected_epoch();
      if (xSemaphoreTake(mutex_state, 200)) {
        setTime( state.gps_read_time );
        xSemaphoreGive(mutex_state);
      }
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
 * DEBUG Task - regularly send messages (for radio testing only).
 * TODO: remove or use DEBUG flag
 */
void Task_message(void *pvParameters) {
  for (;;) {
    if ( rockblock.available() ) {
      rockblock.sendMessage((char*) "Hello world!\0");
    }
    vTaskDelay( pdMS_TO_TICKS( 10000 ) );
  }
}

/*
 * DEBUG Task - Displaying time variables for debugging
 * TODO: remove or use DEBUG flag
 */
void Task_time(void *pvParameters) {
  for (;;) {
    Serial.print("time: "); Serial.print(getTime());
    Serial.print(", start time: "); Serial.print(state.start_time);
    Serial.print(", uptime: "); Serial.print(getTime() - state.start_time, 0);
    Serial.print(", runtime: ");
    Serial.print(int(esp_timer_get_time()/1E6));
    Serial.println();
    vTaskDelay( pdMS_TO_TICKS( 2000 ) );
  }
}


/*
 * Go to sleep when there is nothing left to wait for
 */
void goToSleep() {
  char bfr[255] = {0};
  storage.store( state );
  uint32_t difference = getNextWakeupTime(
    getTime(), state.frequency ) - getTime();
  // set minimum sleep time, just to ensure we are resetting here
  if ( difference < MINIMUM_SLEEP ) { difference = MINIMUM_SLEEP; }
  // set maximim sleep time, so that we don't loose the device
  if ( difference > MAXIMUM_SLEEP ) { difference = MAXIMUM_SLEEP; }
  snprintf(
    bfr, 255, "Going to sleep - reporting interval: %ds, difference: %ds\n",
    (uint32_t) state.frequency, difference);
  Serial.print(bfr);
  // Turn off peripherals
  // Allow for a little bit of wait to make sure expander is actually off.
  if (xSemaphoreTake(mutex_i2c, 1000) == pdTRUE) {
    expander.init();
    xSemaphoreGive(mutex_i2c);
  }
  esp_sleep_enable_timer_wakeup( difference * 1E6 );
  esp_deep_sleep_start();
}

/*
 * Task - Manage timing and logic of all components
 * TODO: Implement more abstract state machine here?
 * TODO: Implement timeouts and retries
 */
void Task_schedule(void *pvParameters) {
  char message[255] = {0};
  for (;;) {

    vTaskDelay( pdMS_TO_TICKS( 1000 ) );

    // Wait until we get a fix and send message
    if ( gps.updated && !state.message_sent ) {
      messages.createPK001(message, state);
      rockblock.sendMessage(message);
      state.message_sent = true;
    }

    // Wait for success and shut done Rockblock
    if ( state.message_sent && rockblock.getSendSuccess() ) {
      rockblock.getLastMessage(message);
      Serial.print("Message: "); Serial.println(message);
      state.rockblock_done = true;
      state.go_to_sleep = true;
    }

    // Shut down when we are timing out, set shorter wakeup time before
    // going to sleep
    if (esp_timer_get_time()/1E6 > state.time_out ) {
      Serial.print("System time out, retry in ");
      Serial.print(RETRY_TIME);
      Serial.println(" seconds.");
      state.frequency = RETRY_TIME;
      state.go_to_sleep = true;
    }

    // Go to sleep
    if ( state.display_off && state.go_to_sleep ) { goToSleep(); }

  }
}

void setup() {
  // ---- Start Serial for debugging --------------
  Serial.begin(115200);
  // ----- Init both HardwareSerial channels ---------------------
  gps_serial.begin(9600, SERIAL_8N1, GPS_SERIAL_RX_PIN, GPS_SERIAL_TX_PIN);
  rockblock_serial.begin(ROCKBLOCK_SERIAL_SPEED, SERIAL_8N1,
    ROCKBLOCK_SERIAL_RX_PIN, ROCKBLOCK_SERIAL_TX_PIN);
  // ---- Start I2C bus for peripherials ----------
  Wire.begin();
  // ---- Restore state
  storage.restore(state);
  // ----- Init Display
  display.begin();
  #if not DEBUG
    display.off();
    state.display_off = true;
  #endif
  // ----- Init Expander and test expander --------
  expander.begin(PORT_EXPANDER_I2C_ADDRESS);
  // ----- Init Blink -----------------------------
  expander.pinMode(10, EXPANDER_OUTPUT);
  expander.pinMode(7, EXPANDER_OUTPUT);
  // Output some useful message
  Serial.println("\nScout buoy firmware v2.0.0-beta");
  Serial.println("https://github.com/tnc-ca-geo/paikea-firmware-new");
  Serial.println("©The Nature Conservancy 2024");
  Serial.println("falk.schuetznemeister@tnc.org\n");
  // ---- Read battery voltage --------------------
  state.bat = readBatteryVoltage();
  Serial.print("Battery: "); Serial.println(state.bat);
  // ---- Give some time to stabilize -------------
  vTaskDelay( pdTICKS_TO_MS(100) );

  /*
   * FreeRTOS setup
   */

  /*
   * Mutex managing shared I2C bus
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
   * Mutex protecting state
   */
  mutex_state = xSemaphoreCreateMutex();

  /*
   * Create and start simple tasks.
   */
  xTaskCreate(&Task_blink, "Task blink", 4096, NULL, 3, NULL);
  xTaskCreate(&Task_gps, "Task gps", 4096, NULL, 12, &gpsTaskHandle);
  xTaskCreate(&Task_schedule, "Task schedule", 4096, NULL, 10, NULL);
  xTaskCreate(&Task_rockblock, "Task rockblock", 4096, NULL, 10,
    &rockblockTaskHandle);
#if DEBUG
  xTaskCreate(&Task_time, "Task time", 4096, NULL, 14, NULL);
  xTaskCreate(&Task_display, "Task display", 4096, NULL, 9, &displayTaskHandle);
#endif
}

/*
 * Nothing to do here, thanks to FreeRTOS.
 */
void loop() {}
