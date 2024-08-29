// libraries
#include <Arduino.h>
#include <Wire.h>
// my libraries
#include <tca95xx.h>
#include <gps.h>
#include <display.h>
#include <loraRockblock.h>
#include <state.h>
#include <scoutMessages.h>
// project
#include "pindefs.h"

// debug flags
#define DEBUG 1
#define SLEEP 1
#define USE_DISPLAY 1

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
// Messages
ScoutMessages scout_messages;

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
    if (state.getGoToSleep() && blink_state) {
      state.setBlinkSleepReady(true);
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
    if ( gps.updated ) { state.setRealTime( gps.get_corrected_epoch(), true ); }
    else state.sync();
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
    if ( state.getDisplayOff() ) {
      Serial.println("Turn display off.");
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        display.off();
        xSemaphoreGive(mutex_i2c);
        vTaskDelete( displayTaskHandle );
      }
    } else {
      time_t time = state.getRealTime();
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
 */
void Task_rockblock(void *pvParameters) {
  if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
    rockblock.toggle(true);
    xSemaphoreGive(mutex_i2c);
  }
  // We do not need to rejoin unless we are using a different device. If we
  // create a Lora class for IoT we need to rewrite.
  // rockblock.configure();
  // rockblock.beginJoin();
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
      sprintf(bfr, "GPS updated: %.5f, %.5f\n", gps.lat, gps.lng);
      state.setGpsDone(true);
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
    // snprintf(bfr, 255,
    //  "Sleep ready state %d: blink %d, gps %d, rockblock %d\n",
    //  state.getGoToSleep(), state.getBlinkSleepReady(),
    //  state.getGpsDone(), state.getRockblockDone() );
    // Serial.print(bfr);
    // check whether to initiate sleep
    if (state.getSystemSleepReady()) {
      Serial.println("Going to sleep");
      // Sets expander to init state which uses hopefully the least power. All
      // pins configured as inputs
      if (xSemaphoreTake(mutex_i2c, 200) == pdTRUE) {
        expander.init();
        xSemaphoreGive(mutex_i2c);
      }
      // Print next send time to serial.
      // snprintf(bfr, 255, "Next send time %d -> %d\n", state.getRealTime(),
      // next_send_time(state.getRealTime(), 300));
      // Set expected wake up time and turn display off
      // Set values that need to survive sleep but NOT a cold start
      // rtc_expected_wakeup = state.getRealTime() + SLEEP_TIME;
      state.setDisplayOff( true );
      // Give some time to turn display off
      // TODO: check actual state
      vTaskDelay( pdMS_TO_TICKS(300) );
      // Store state values into persistent storage
      state.persist();
      // Give some extra time for everything to settle down.
      vTaskDelay( pdMS_TO_TICKS( 250 ) );
      Serial.print("Frequency: "); Serial.print(state.getFrequency());
      Serial.print(", next send time: ");
      Serial.print(state.getNextSendTime());
      int64_t difference =
        state.getNextSendTime() - state.getRealTime();
      Serial.print(", difference: ");
      Serial.println(difference);
      esp_sleep_enable_timer_wakeup( difference * 1E6 );
      esp_deep_sleep_start();
    }
    // Check for sleep once a second.
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
  }
}

/*
 * Task regularly sending messages (for radio only testing).
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
 * Task - Manage timing and logic of all components.
 */
void Task_schedule(void *pvParameters) {
  char message[255] = {0};
  for (;;) {

    vTaskDelay( pdMS_TO_TICKS( 1000 ) );

    if ( gps.updated && !state.getMessageSent() ) {
      scout_messages.createPK001(state, message);
      rockblock.sendMessage(message);
      state.setMessageSent(true);
    }

    if ( state.getMessageSent() && rockblock.getSendSuccess() ) {
      state.setRockblockDone(true);
    }

    if ( gps.updated && state.getMessageSent() ) {
      // We are setting a flag that all components can go into sleep state
      state.setGoToSleep(true);
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
  // ---- Load state
  state.init();
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