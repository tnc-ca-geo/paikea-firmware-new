/*
 * Light weight GPS class relying non Arduino TinyGPS for parsing
 */
#ifndef __GPS_H__
#define __GPS_H__
#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>
// project
#include <tca95xx.h>


class Gps {

private:
    char read_buffer[255];
    time_t time_to_epoch(TinyGPSDate date, TinyGPSTime time);
    HardwareSerial* serial;
    Expander* expander;
    TinyGPSPlus gps_parser;
    uint8_t enable_pin;
    bool enabled = false;
    time_t start_time = 0;

public:
    // Variables
    time_t epoch = 0;
    // Time when GPS was read, to correct for delayed processing.
    time_t gps_read_system_time = 0;
    bool updated = false;
    float lat;
    float lng;
    // implement those mainly for compatibility
    float speed;
    float heading;
    // Constructor
    Gps(Expander &expander, HardwareSerial &seria, uint8_t enable_pin);
    // Methods
    // Return epoch corrected by the time passed since last GPS read.
    time_t get_corrected_epoch();
    void enable();
    void disable();
    void loop();
};

#endif /* __GPS_H__ */
