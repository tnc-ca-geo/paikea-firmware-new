/* -----------------------------------------------------------------------------
 * Firmware for Scout buoy device.
 *
 * A much simpler firmware for the Scout buoy than the original (v2)
 * MicroPython version by Matt Arcady. Focusses on simplicity, timing,
 * and behavior transparent and predictable to the user.
 *
 * This firmware is written in C++ using the Arduino framework.
 * Version 3.1.1-beta
 *
 * https://github.com/tnc-ca-geo/paikea-firmware-new.git
 *
 * falk.schuetzenmeister@tnc.org
 * © The Nature Conservancy 2025
 *
 * Future improvements:
 * - we still relay heavily on third party libraries, sometimes only marginally
 * we might replace some of them
 * - we still use Matt Arcady's message types even though in slight variations
 * - since sending takes quite some time we could keep reading GPS and get cog
 * and sog
 * - binary message for shorter and cheaper messages
 * - fully implement hardware abstraction
 * - correction factor for known time drift
 * -----------------------------------------------------------------------------
 */
// debug flags
#define USE_DISPLAY 0
#define DEBUG 0
#define TIMING 0

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#ifdef DEBUG
#include <Preferences.h>
#endif
// my libraries
#include "pindefs.h" // contains hardware specs and defined defaults
// project
#include <tca95xx.h>
#include <gps.h>
#include <display.h>
#include <rockblock.h>
#include <stateType.h>
#include <scoutMessages.h>
#include <storage.h>
#include <helpers.h>


// printf templates
#define SLEEP_TEMPLATE "Going to sleep:\n - reporting interval: %ds \
  \n - difference: %ds\n - retries: %d\n - message status: %s\n"
#define ERROR_SLEEP_TEMPLATE ("Going to sleep because of a system error.\n" \
  "Retry in %d seconds.")
#define GPS_MESSAGE_TEMPLATE "GPS updated: %.05f, %.05f after %d seconds\n"

std::map<messageType, String> scoutMessageTypeLabels = {
  {NORMAL, "NORMAL"}, {FIRST, "FIRST"}, {WAKE_UP, "SLEEP WAKE UP"},
  {CONFIG, "CONFIG"}, {ERROR, "ERROR"}
};

/*
 * FreeRTOS setup
 */
// Use to protect I2C shared between display and expander.
static SemaphoreHandle_t mutex_i2c;
// Use to protect state object if variable update is not atomic, e.g. buffers or
// long types
static SemaphoreHandle_t mutex_state;
// Create TaskhHandles, only needed if the task is referenced outside the task
static TaskHandle_t rockblockTaskHandle = NULL;
static TaskHandle_t gpsTaskHandle = NULL;
static TaskHandle_t blinkTaskHandle = NULL;

/*
 * Hardware and peripheral objects
 */
HardwareSerial gps_serial(1);
// Defined in hal.h
RockblockSerial rockblock_serial = RockblockSerial();
// Port Expander using i2c
Expander expander = Expander(Wire, PORT_EXPANDER_I2C_ADDRESS);
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
#ifdef DEBUG
Preferences preferences;
#endif

/*
 * Keep functions that interact with ESP in main.cpp for now
 */

/*
 * Read RTC system time and return UNIX epoch.
 */
time_t getTime() {
  struct timeval tv_now = {0};
  gettimeofday(&tv_now, NULL);
  return tv_now.tv_sec;
}

/*
 * Set RTC system time. We relay on the ESP32 internal clock to continue timing
 * through deep sleep. However, the time drift is remarkable. So far we had only
 * hardware where the time has been shorter than expected
 */
void setTime(time_t time) {
  struct timeval new_time = {0};
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
    vTaskDelay( pdMS_TO_TICKS( 20 ) );
  }
  // adjust for voltage divider
  return readings * (BATT_R_UPPER + BATT_R_LOWER)/BATT_R_LOWER;
}

/*
 * Go to sleep. If error is true, we treat this as a reaction to a systen error.
 *
 * :params bool error: Whether to go to sleep because of an error.
 * :return type void:
 */
void goToSleep(bool error=false) {
  char bfr[128] = {0};
  uint32_t difference = ERROR_SLEEP_DIFFERENCE;
  // Store data needed on wakeup
  storage.store( state );
  // Output a message before sleeping
  if ( xSemaphoreTake(mutex_i2c, 1000) == pdTRUE ) {
    // Clear display, since we don't want to show anything while sleeping
    display.off();
    // delete Rockblock task
    vTaskDelete( rockblockTaskHandle );
    // Set port expander to known state, i.e. peripherals off, holding RB
    // enable pin HIGH.
    expander.init();
    // turn Rockblock off
    rockblock.toggle(false);
    xSemaphoreGive(mutex_i2c);
  }
  if (error) {
    snprintf(
      bfr, 128, ERROR_SLEEP_TEMPLATE, difference,
      scoutMessageTypeLabels[state.mode]);
  } else {
    difference = helpers::getSleepDifference( state, getTime() );
    snprintf(
      bfr, 128, SLEEP_TEMPLATE, state.interval, difference, state.retries,
      scoutMessageTypeLabels[state.mode]);
  }
  Serial.println(bfr);
  Serial.print("Expected wakeup (UTC): ");
  strftime(bfr, 32, "%F %T", gmtime(&state.expected_wakeup));
  Serial.println(bfr);
  esp_sleep_enable_timer_wakeup( difference * 1E6 );
  try {
    esp_deep_sleep_start();
  } catch (...) {
    Serial.println("Sleep failure.");
  }
}

/*
 * Define FreeRTOS tasks
 */

/*
 * Blink LED 1 and 0 as simple (and only very hard to see) system indicator.
 * Needs mutex since the I2C bus is shared with display and I/O expander.
 *
 * Use LED 1 and LED 0 the synchronously because they cannot be distinguished
 * from the outside of the enclosure.
 *
 * OFF (sleep, turned off by resetting the expander in the sleep function)
 */
void Task_blink(void *pvParamaters) {
  bool blink_state = HIGH;
  uint8_t ctr = 0;
  while (true) {
    if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
      expander.digitalWrite(LED01, blink_state);
      expander.digitalWrite(LED00, blink_state);
      xSemaphoreGive(mutex_i2c);
    }
    // blink quickly before GPS fix, slowly when attempt sending
    blink_state = !(ctr % (state.gps_done ? 15 : 3));
    ctr++;
    // Delay affects blink speed, intervals are multiples of that number
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
  }
}

/*
 * DEBUG ONLY: Print state info to LilyGO display. Mutex to protect I2C bus.
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
 * Running the Iridium radio loop once.
 */
void Task_rockblock(void *pvParameters) {
  // Give it some time to be suspended
  vTaskDelay( pdMS_TO_TICKS( 200 ) );
  Serial.println("Start radio loop");
  // Give some time for the peripherials to stabilize.
  // It might hang up if we start too early
  vTaskDelay( pdMS_TO_TICKS( 200 ) );
  if (xSemaphoreTake(mutex_i2c, 100) == pdTRUE) {
    rockblock.toggle(true);
    xSemaphoreGive(mutex_i2c);
  }
  while (true) {
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
    rockblock.loop();
  }
}

/*
 * Run GPS
 */
void Task_gps(void *pvParameters) {
  if (xSemaphoreTake(mutex_i2c, 100) == pdTRUE) {
    gps.enable();
    xSemaphoreGive(mutex_i2c);
  }
  while(true) {
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
    gps.loop();
  }
}

/*
 * DEBUG Task - Display time variables for debugging
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
 */
void Task_main_loop(void *pvParameters) {
  // setup
  mainFSM fsmState = AWAKE;
  // use as needed
  char bfr[255] = {0};
  // use for timed action or output in increaments of 100ms, e.g. while waiting
  // for state change
  uint16_t ctr = 0;

  while (true) {
    // Check whether port expander is available by writing and reading to an
    // unused port.
    //
    // Go to sleep on failure which means peripherals might not be powered.
    // This is an important indicator when MC board is connected to a computer
    // via USB. This might result in a situation where the MC is powered while
    // the peripherials are not.
    if (xSemaphoreTake(mutex_i2c, 100) == pdTRUE) {
      if (!expander.check()) {
        Serial.println("\nERROR: Board peripherials did not respond.\n"
          "Please check power/batteries, e.g. turn ON and press RESET.\n");
        fsmState = ERROR_SLEEP;
      };
      xSemaphoreGive(mutex_i2c);
    }

    switch (fsmState) {

      case AWAKE: {
        vTaskResume( gpsTaskHandle );
        fsmState = WAIT_FOR_GPS;
        break;
      }

      case WAIT_FOR_GPS: {
        bool timeout_test = getRunTime() > GPS_TIME_OUT;
        // update state
        fsmState = helpers::processGpsFix(
          state, gps, getTime(), timeout_test);
        // set read time clock and some output
        if (gps.updated) {
          setTime( gps.get_corrected_epoch() );
          snprintf(bfr, 255, GPS_MESSAGE_TEMPLATE, state.lat, state.lng,
            getRunTime());
          Serial.println(bfr);
        } else if (timeout_test) {
          Serial.println( "GPS: Timeout." );
        } else if (ctr % 50 == 0) { Serial.println("GPS: Waiting for fix."); }

        // State transitions affecting hardware and queue message
        if (fsmState == WAIT_FOR_RB) {
          // stop GPS
          vTaskDelete(gpsTaskHandle);
          // start Rockblock
          vTaskResume(rockblockTaskHandle);
          // turn off GPS hardware
          if (xSemaphoreTake(mutex_i2c, 100) == pdTRUE) {
            gps.disable();
            xSemaphoreGive(mutex_i2c);
          }
          // send message and update FSM
          scoutMessages::createPK101(bfr, state);
          rockblock.sendMessage(bfr);
        }
        break;
      };

      case WAIT_FOR_RB: {
        // check whether we are timing out
        if (getRunTime() > SYSTEM_TIME_OUT) {
          Serial.println("\nRB: Timeout\n");
          state.retries--;
          state.retry = state.retries > 0;
          fsmState = SLEEP_READY;
        } else {
          // Check for incoming messages
          rockblock.getLastIncoming(bfr);
          // Determine next state, systemState will be updated as a side effect
          // I considered passing a reference to the rockblock instance but
          // that makes testing harder, therefore passing only select values.
          fsmState = helpers::processRockblockMessage(
            state, bfr, rockblock.sendSuccess,
            rockblock.state == SENDING || rockblock.state == INCOMING);
          if (fsmState == SLEEP_READY) {
            state.retries = 3;
            Serial.println("\nRB: Send success");
            Serial.print("RB: Incoming message - ");
            Serial.println(bfr); Serial.println();
          }
        }
        break;
      };

      // normal sleep
      case SLEEP_READY: {
        goToSleep();
        break;
      }

      // Go to sleep because there has been a system error
      case ERROR_SLEEP: {
        goToSleep(true);
        break;
      }

    }
    ctr++;
    vTaskDelay( 100 );
  }
}

/*
 * Hard timeout to recover a hangup system. Hopefully this will never be
 * triggered. Graceful timeout will be handled by the main task.
 */
void Task_timeout(void *pvParameters) {
  while (true) {
    // Give it an extra 30 seconds so that we can catch timeout gracefully.
    if ( getRunTime() > SYSTEM_TIME_OUT + 30 ) {
      char bfr[64] = {0};
      snprintf(bfr, 64, "HARD RESET after %d seconds", SYSTEM_TIME_OUT + 30);
      Serial.println(bfr);
      // Not using the complicated sleep function here since it might
      // have dependencies not met. Just reset and hope system runs successfully
      // on the next try. This will trigger a turn on message on the user side

#ifdef DEBUG
    // Store the number of hard resets for debugging
    preferences.begin("debug", false);
    int resets = preferences.getInt("hard_resets");
    resets++;
    preferences.putInt("hard_resets", resets);
    preferences.end();
#endif

      esp_restart();
    }
    // no hurry here
    vTaskDelay( pdMS_TO_TICKS( 5000 ) );
  }
}

/*
 * Setup
 */
void setup() {
  // --- Go slow for power consumption since a 32bit system with 240Mhz is
  // --- overkill for this system that is mostly waiting around
  setCpuFrequencyMhz(10);
  // Task to monitor the system, will reset the system if we not finish in time
  xTaskCreate(&Task_timeout, "Task timeout", 4096, NULL, 10, NULL);
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
  // ---- Init Display: if not used it should be turned be off properly, it
  // ---- might have random content on power on
  display.begin();
  display.off();
  // Output some useful message
  Serial.println("\nScout buoy firmware v3.1.1-beta");
  Serial.println("https://github.com/tnc-ca-geo/paikea-firmware-new");
  Serial.println("falk.schuetzenmeister@tnc.org");
  Serial.println("\n© The Nature Conservancy 2025\n");
  Serial.print("reporting interval: "); Serial.println(state.interval);
  // ---- Read battery voltage --------------------
  state.bat = readBatteryVoltage();
  Serial.print("battery: "); Serial.println(state.bat);

#ifdef DEBUG
  preferences.begin("debug", false);
  Serial.print("Restarts: ");
  int restarts = preferences.getDouble("restarts", 0);
  if ( state.first_run ) {
    restarts++;
  }
  Serial.println(restarts);
  preferences.putDouble("restarts", restarts);
  Serial.print("Sleep failures: ");
  Serial.println(preferences.getInt("hard_resets"));
  preferences.end();
#endif

  Serial.println();
  // ----- Init IO Expander -----------------------
  expander.init();
  // ----- Init LEDs for Blink --------------------
  expander.pinMode(10, EXPANDER_OUTPUT);
  expander.pinMode(7, EXPANDER_OUTPUT);

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
  xTaskCreate(&Task_blink, "Task blink", 4096, NULL, 3, &blinkTaskHandle);
  xTaskCreate(&Task_gps, "Task gps", 4096, NULL, 12, &gpsTaskHandle);
  // Don't run before system is up
  vTaskSuspend(gpsTaskHandle);
  xTaskCreate(&Task_rockblock, "Task rockblock", 4096, NULL, 10,
    &rockblockTaskHandle);
  // Don't run radio until needed
  vTaskSuspend(rockblockTaskHandle);
  xTaskCreate(&Task_main_loop, "Task main loop", 4096, NULL, 10, NULL);
#if TIMING
  xTaskCreate(&Task_time, "Task time", 4096, NULL, 14, NULL);
#endif
#if USE_DISPLAY
  xTaskCreate(&Task_display, "Task display", 4096, NULL, 9, NULL);
#endif
}

/*
 * Main loop: Nothing to do here, thanks to FreeRTOS.
 */
void loop() {}
