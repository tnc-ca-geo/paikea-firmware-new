/*
 * This file contains some helper functions I don want to have in main.cpp for
 * better readibility.
 */
#include <Arduino.h>

/*
 * Convert a timestamp to a printable buffer
 */
void timestamp_to_string(time_t timestamp, char *bfr) {
    strftime(bfr, 20, "%F\n%T", gmtime( &timestamp ));
}