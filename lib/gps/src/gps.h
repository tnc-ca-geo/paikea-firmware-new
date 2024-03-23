#ifndef __GPS_H__
#define __GPS_H__
#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>
// project
#include <tca95xx.h>
#include <gps.h>


class Gps {

private:
    char read_buffer[255];
    time_t time_to_epoch(TinyGPSDate date, TinyGPSTime time);
    HardwareSerial* serial;
    Expander* expander;
    TinyGPSPlus gps_parser;
    bool enabled = false;

public:
    // Variables
    time_t epoch = 0;
    uint64_t gps_read_system_time = 0xFFFFFFFFFFFFFFFF;
    bool updated = false;
    float lat;
    float lng;
    // Constructor
    Gps(Expander &expander, HardwareSerial &serial);
    // Methods
    void enable();
    void disable();
    void loop();
    void readln(char *buffer);
    // turn GPS on or off
    void toggle(bool gps_on);
};

#endif /* __GPS_H__ */
