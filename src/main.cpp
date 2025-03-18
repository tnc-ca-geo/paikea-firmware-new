/* -----------------------------------------------------------------------------
 * Firmware for Scout buoy device.
 *
 * This is a very much stripped down, simpler version than the original (v2)
 * MicroPython by Matt Arcady. We focus on simplicity, timing, and behavior that
 * is transparent and predictable to the user.
 *
 * https://github.com/tnc-ca-geo/paikea-firmware-new.git
 *
 * falk.schuetzenmeister@tnc.org
 * © The Nature Conservancy 2025
 * -----------------------------------------------------------------------------
 */

/*
 * We still heavily relay on third party libraries (mostly from the Arduino
 * ecosystsem that we should replace in the future with your own, IF this code
 * will be widely used in production.
 */
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
// my libraries
#include <tca95xx.h>
#include <gps.h>
#include <display.h>
#include <rockblock.h>
#include <stateType.h>
#include <scoutMessages.h>
#include <storage.h>
#include <helpers.h>
// project
#include "pindefs.h" // contains hardware specs and defined defaults

// debug flags
#define DEBUG 1
#define SLEEP 1
#define USE_DISPLAY 1

// printf templates
#define SLEEP_TEMPLATE "Going to sleep:\n - reporting interval: %ds\n - difference: %ds\n"

/*
 * Define states for Main FSM
 */
enum mainFSM {
  AWAKE,
  WAITING_FOR_GPS,
  WAITING_FOR_RB,
  RB_DONE,
  SLEEP_READY
};

/*
 * FreeRTOS setup
 */
// Use to protect I2C shared between display and expander.
static SemaphoreHandle_t mutex_i2c;
// Use to protect state object if not atomic, ideally we would modify state
// object only in one place
static SemaphoreHandle_t mutex_state;
// Create TaskhHandles, only needed if the task is deleted or suspended outside
// of the task itself.
static TaskHandle_t rockblockTaskHandle = NULL;
static TaskHandle_t gpsTaskHandle = NULL;

/*
 * Hardware and peripheral objects
 */
HardwareSerial gps_serial(1);
// Defined in hal.h
RockblockSerial rockblock_serial = RockblockSerial();
// Port Expander using i2c
Expander expander = Expander(Wire);
// GPS using UART
Gps gps = Gps(expander, gps_serial, PORT_EXPANDER_GPS_ENABLE_PIN);
// Display using i2c, for development only.
LilyGoDisplay display = LilyGoDisplay(Wire);
// Rockblock or Lora Simulation
Rockblock rockblock = Rockblock(
  expander, rockblock_serial, PORT_EXPANDER_ROCKBLOCK_ENABLE_PIN);
// State object
systemState state;
// Storage
ScoutStorage storage = ScoutStorage();
// Messages
ScoutMessages messages;

/*
 * Read RTC system time and return UNIX epoch.
 */
time_t getTime() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return tv_now.tv_sec;
}

/*
 * Set RTC system time. We relay on the ESP32 internal clock to continue timing
 * through deep sleep.
 */
void setTime(time_t time) {
  struct timeval new_time;
  new_time.tv_sec = time;
  settimeofday(&new_time, NULL);
}

/*
 * Shorthand for getting system uptime since wakeup (or power on).
 */
uint16_t getRunTime() { return round(esp_timer_get_time() / 1E6); }

/*
 * Read battery voltage (on wakeup, no reason to get fancy here)
 */
float readBatteryVoltage() {
  float readings = 0;
  for (uint8_t i=0; i<10; i++) {
    // devide through 1000 and through 10 for 10 readings
    readings += (float) analogReadMilliVolts(BATT_ADC)/10000;
    vTaskDelay( pdMS_TO_TICKS( 10 ) );
  }
  // adjust for voltage divider
  return readings * (BATT_R_UPPER + BATT_R_LOWER)/BATT_R_LOWER;
}

/*
 * Rather simple for now, might become a more complex function for state
 * transition.
 */
void update_state_from_gps(systemState &state, Gps gps) {
  state.lat = gps.lat;
  state.lng = gps.lng;
  state.heading = gps.heading;
  state.speed = gps.speed;
  state.gps_done = true;
  state.gps_read_time = gps.get_corrected_epoch();
  if (state.start_time == 0) {
    state.start_time = state.gps_read_time - getTime();
  }
}

/*
 * Go to sleep
 */
void goToSleep() {
  char bfr[128] = {0};
  int difference = getSleepDifference( state, getTime() );
  // Store data needed on wakeup
  storage.store( state );
  // Output a message before sleeping
  snprintf(bfr, 128, SLEEP_TEMPLATE, state.interval, difference);
  Serial.print(bfr);
  // give it an extra second
  vTaskDelay( pdMS_TO_TICKS (1000) );
  if ( xSemaphoreTake(mutex_i2c, 1000) == pdTRUE ) {
    // Clear display, since we don't want to show anything while sleeping
    display.off();
    // remove Rockblock task
    rockblock.toggle(false);
    vTaskDelete( rockblockTaskHandle );
    // Set port expander to known state, i.e. peripherals off
    expander.init();
    xSemaphoreGive(mutex_i2c);
  }
  esp_sleep_enable_timer_wakeup( difference * 1E6 );
  esp_deep_sleep_start();
}

/*
 * Define FreeRTOS tasks
 */

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
  while (true) {
    if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
      expander.digitalWrite( LED01, blink_state || state.gps_done );
      expander.digitalWrite(
        LED00, state.gps_done && (blink_state || state.rockblock_done ));
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
  char out_string[44] = {0};
  char time_string[22] = {0};
  time_t time = 0;
  while (true) {
    time = getTime();
    strftime(time_string, 22, "%F\n  %T", gmtime( &time ));
    snprintf(
      out_string, 50, "%s\nsignal %3d\n", time_string, state.signal);
    if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
      display.set(out_string);
      xSemaphoreGive(mutex_i2c);
    }
    vTaskDelay( pdMS_TO_TICKS( 50 ) );
  }
}

/*
 * Running the radio.
 */
void Task_rockblock(void *pvParameters) {
  // Give it some time since we suspend task after creation. Not a great
  // solution but good for now.
  vTaskDelay( pdMS_TO_TICKS( 100 ) );
  Serial.println("Start radio loop");
  if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
    rockblock.toggle(true);
    xSemaphoreGive(mutex_i2c);
  }
  while (true) {
    rockblock.loop();
    // to display signal strength
    state.signal = rockblock.getSignalStrength();
    vTaskDelay( pdMS_TO_TICKS( 50 ) );
  }
}

/*
 * Run GPS
 */
void Task_gps(void *pvParameters) {
  // give it some wait to be suspended after creation
  vTaskDelay( pdMS_TO_TICKS( 100 ) );
  unsigned int ctr=0;

  if (xSemaphoreTake(mutex_i2c, 100) == pdTRUE) {
    gps.enable();
    xSemaphoreGive(mutex_i2c);
  }

  while(true) {
    // show every 50 * 100 ms = 5s
    if (ctr % 50 == 0) { Serial.println("GPS: Waiting for fix."); }
    gps.loop();
    ctr++;
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
  }

}

/*
 * DEBUG Task - Displaying time variables for debugging
 * TODO: remove or use DEBUG flag
 */
void Task_time(void *pvParameters) {
  while (true) {
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
 * Task - Manage timing and logic of all components
 * TODO: Implement timeouts and retries
 */
void Task_main_loop(void *pvParameters) {
  mainFSM fsm_state = AWAKE;
  // use as needed
  char bfr[255] = {0};
  for (;;) {

    switch (fsm_state) {

      case AWAKE: {
        // TODO: move some of the preparation from setup here?
        fsm_state = WAITING_FOR_GPS;
        vTaskResume(gpsTaskHandle);
        break;
      }

      case WAITING_FOR_GPS: {
        bool time_out_test = getRunTime() > GPS_TIME_OUT;
        if (gps.updated) {
          Serial.println("GPS: Fix acquired.");
          update_state_from_gps(state, gps);
          setTime( state.gps_read_time );
        }
        if (time_out_test) {
          Serial.println("GPS: Timeout.");
          // add the current time to the failed GPS message but continue with
          // power on at 0 time
          state.gps_read_time = getTime();
        }
        if (gps.updated || time_out_test) {
          // stop GPS task and turn off GPS hardware
          vTaskDelete(gpsTaskHandle);
          if (xSemaphoreTake(mutex_i2c, 100) == pdTRUE) {
            gps.disable();
            xSemaphoreGive(mutex_i2c);
          }
          messages.createPK001(bfr, state);
          rockblock.sendMessage(bfr);
          fsm_state = WAITING_FOR_RB;
        }
        break;
      };

      case WAITING_FOR_RB: {
        // Make sure we don't interrupt a successful message retrieval process
        bool waitForRB = (
          rockblock.state == SENDING || rockblock.state == INCOMING);
        vTaskResume(rockblockTaskHandle);
        if (rockblock.sendSuccess) { fsm_state = RB_DONE; }
        // gracefully shutdown RB before timoutTask would shut it it down no
        // matter what
        else if (getRunTime() > TIME_OUT and !waitForRB) {
          Serial.println("TIMEOUT");
          state.retries = state.retries - 1;
          fsm_state = SLEEP_READY;
        }
        break;
      };

      case RB_DONE: {
        char bfr[255] = {0};
        Serial.println("SUCCESS");
        state.send_success = true;
        // used only for blink state
        state.rockblock_done = true;
        state.retries = 3;
        rockblock.getLastIncoming(bfr);
        if (bfr[0] != '\0') {
          Serial.print("Incoming message: "); Serial.println(bfr);
          if ( messages.parseIncoming(state, bfr) ) {
            Serial.print("New reporting time: ");
            Serial.println(state.interval);
          }
        }
        fsm_state = SLEEP_READY;
        break;
      };

      case SLEEP_READY: {
        goToSleep();
        break;
      }

    }
    vTaskDelay( pdMS_TO_TICKS( 50 ) );
  }
}

/*
 * Hard timeout to recover hangup system, consider this a gentle watchdog,
 * hopefully that will never happen.
 */
void Task_timeout(void *pvParameters) {
  for (;;) {
    if ( getRunTime() > TIME_OUT + 20 ) {
      Serial.print("HARD TIMEOUT after "); Serial.print(TIME_OUT);
      Serial.println(" seconds.");
      state.retries = state.retries - 1;
      Serial.print("Retries left: "); Serial.println(state.retries);
      goToSleep();
    }
    // no hurry here
    vTaskDelay( pdMS_TO_TICKS( 2000 ) );
  }
}

/*
 * Setup
 */

void setup() {
  // ---- Start Serial for debugging --------------
  Serial.begin(115200);
  // ----- Init both HardwareSerial channels ---------------------
  gps_serial.begin(9600, SERIAL_8N1, GPS_SERIAL_RX_PIN, GPS_SERIAL_TX_PIN);
  rockblock_serial.begin(ROCKBLOCK_SERIAL_SPEED, SERIAL_8N1,
    ROCKBLOCK_SERIAL_RX_PIN, ROCKBLOCK_SERIAL_TX_PIN);
  // ---- Start I2C bus for peripherials ----------
  Wire.begin();
  // ---- set state defaults
  // Reporting times need to be changed via downlink message and will only be
  // persisted until next power off
  state.interval = DEFAULT_INTERVAL;
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
  Serial.println("\nScout buoy firmware v3.0.0-pre-alpha");
  Serial.println("https://github.com/tnc-ca-geo/paikea-firmware-new");
  Serial.println("falk.schuetzenmeister@tnc.org");
  Serial.println("\n© The Nature Conservancy 2025\n");
  Serial.print("reporting time: "); Serial.println(state.interval);
  // ---- Read battery voltage --------------------
  state.bat = readBatteryVoltage();
  Serial.print("battery: "); Serial.println(state.bat);
  Serial.println();

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
  // Don't run before system is up
  vTaskSuspend(gpsTaskHandle);
  xTaskCreate(&Task_rockblock, "Task rockblock", 4096, NULL, 10,
    &rockblockTaskHandle);
  // Don't run radio until needed
  vTaskSuspend(rockblockTaskHandle);
  xTaskCreate(&Task_main_loop, "Task main loop", 4096, NULL, 10, NULL);
  xTaskCreate(&Task_timeout, "Task timeout", 4096, NULL, 10, NULL);
#if DEBUG
  // xTaskCreate(&Task_time, "Task time", 4096, NULL, 14, NULL);
  xTaskCreate(&Task_display, "Task display", 4096, NULL, 9, NULL);
#endif
}

/*
 * Main loop: Nothing to do here, thanks to FreeRTOS.
 */
void loop() {}
