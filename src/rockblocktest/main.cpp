/*
 * Test sending and receiving Rockblock messages outside of the main app.
 */
#include <Arduino.h>
#include <Wire.h>
// project libraries
#include <display.h>
#include <pindefs.h>
#include <rockblock.h>
#include <hal.h>


RockblockSerial rockblock_serial = RockblockSerial();
// Port Expander using i2c
Expander expander = Expander(Wire);
// GPS using UART
Rockblock rockblock = Rockblock(expander, rockblock_serial);

static TaskHandle_t messageTaskHandle = NULL;

/*
 * Create test message if Rockblock available
 */
void TaskMessage(void *pvParameters) {
    char message[64] = {0};
    rockblock.toggle(true);
    for (;;) {
        if (rockblock.available()) {
            snprintf(message, 64, "Hello world!");
            rockblock.sendMessage(message);
            vTaskDelete(messageTaskHandle);
        }
        vTaskDelay( pdMS_TO_TICKS(5000) );
    }
}


void TaskRockblock(void *pvParameters) {
    for (;;) {
        rockblock.loop( esp_timer_get_time() );
        vTaskDelay( pdMS_TO_TICKS( 300 ) );
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    expander.begin(PORT_EXPANDER_I2C_ADDRESS);
    rockblock_serial.begin(ROCKBLOCK_SERIAL_SPEED, SERIAL_8N1,
        ROCKBLOCK_SERIAL_RX_PIN, ROCKBLOCK_SERIAL_TX_PIN);
    rockblock.toggle(true);
    xTaskCreate(&TaskRockblock, "Task rockblock", 4096, NULL, 0, NULL);
    xTaskCreate(&TaskMessage, "Task message", 4096, NULL, 1, NULL);
}

void loop() {
    vTaskDelay( pdMS_TO_TICKS(1000) );
}
