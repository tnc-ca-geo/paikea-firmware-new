/*
 * This is a script to test the power system. Showing voltage and discharge the
 * battery. It works on the standard Scout buoys as well as on the special
 * build test bed which follows the schematic of the Scout buoys.
 */
#include <Arduino.h>
#include <Wire.h>
// project libraries
#include <display.h>
#include <pindefs.h>


LilyGoDisplay display = LilyGoDisplay(Wire);
float startVoltage = 0;
float batteryVoltage = 0;


// Shared with main code, move to library
float readBatteryVoltage() {
  float readings = 0;
  for (uint8_t i=0; i<10; i++) {
    readings += (float) analogReadMilliVolts(BATT_ADC)/10000;
    vTaskDelay( pdMS_TO_TICKS( 10 ));
  }
  return readings * (BATT_R_UPPER + BATT_R_LOWER)/BATT_R_LOWER;
}


void Task_display(void *pvParameters) {
    char out_string[44] = {0};
    time_t time;
    uint16_t minutes;
    uint8_t seconds;
    uint8_t hours;
    for(;;) {
        time = esp_timer_get_time()/1E6;
        minutes = int(time/60);
        hours = int(minutes/60);
        minutes = minutes % 60;
        seconds = time % 60;
        snprintf(out_string, 44, "%5.2f Volt\n%5.2f Volt\n\n%4d:%02d:%02d",
            batteryVoltage, startVoltage, hours, minutes, seconds);
        display.set(out_string);
        vTaskDelay( pdMS_TO_TICKS( 200 ) );
    }
}

void Task_read_voltage(void *pvParameters) {
    for(;;) {
        batteryVoltage = readBatteryVoltage();
        if ( startVoltage == 0 ) startVoltage = batteryVoltage;
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Hello world");
    Wire.begin();
    display.begin();
    xTaskCreate(&Task_display, "Task display", 4096, NULL, 0, NULL);
    xTaskCreate(&Task_read_voltage, "Task read voltage", 4096, NULL, 1, NULL);
}

void loop() {}