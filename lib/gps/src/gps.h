#ifndef GPS
#define GPS
#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

class Gps {

private:
    char read_buffer[255];
    time_t time_to_epoch(TinyGPSDate date, TinyGPSTime time);

public:
    uint64_t epoch = 0;
    uint64_t gps_read_system_time = 0xFFFFFFFFFFFFFFFF;
    // Enable is currently external, but it would be cool to pass in a function
    // that allows enabling by the library.
    Gps();
    void begin(uint8_t rx_pin, uint8_t tx_pin);
    void loop();
    void readln(char *buffer);
};

#endif
