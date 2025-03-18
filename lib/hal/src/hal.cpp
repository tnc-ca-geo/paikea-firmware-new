/*
 * This is a rudimentary hardware abstraction layer. If fully implemented it
 * would allow us to run the firmware on different hardware platforms, i.e. on
 * a desktop computer for testing.
 *
 * TODO: Fully implement.
 */
#include <hal.h>
// Only include if we run on actual hardware
#ifndef NATIVE
#include <Arduino.h>
#endif

#ifndef NATIVE

#define ROCKBLOCK_SERIAL_RX_PIN 34
#define ROCKBLOCK_SERIAL_TX_PIN 25
#define ROCKBLOCK_SERIAL_SPEED 19200

/*
 * We need to keep this in global scope even though there might be a chance for
 * conflict elsewhere.
 */
HardwareSerial hws(2);

RockblockSerial::RockblockSerial() {
    this->serial = &hws;
};

void RockblockSerial::begin(uint16_t serialSpeed, int serial8N1, uint8_t rxPin,
    uint8_t txPin) {
        this->serial->begin(serialSpeed, serial8N1, rxPin, txPin);
        // Serial.println("ROCKBLOCK SERIAL BEGIN SUCCESS");
    }

void RockblockSerial::print(const char *bfr) { this->serial->print(bfr); }

char RockblockSerial::read() { return this->serial->read(); }

bool RockblockSerial::available() { return this->serial->available(); }

#endif