/*
 * This file contains some helper functions I don want to have in main.cpp for
 * better readibility.
 */
#include <Arduino.h>

/*
 * Calculate the next fix and send time
 */

time_t next_send_time(time_t now, time_t delay) {
    time_t full_hour = int(now/3600) * 3600;
    uint8_t cycles = int(now % 3600/delay);
    return full_hour + (cycles + 1) * delay;
}