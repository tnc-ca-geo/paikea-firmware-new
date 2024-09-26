/*
 * Define the state type here for better testibility
 */
#ifndef __SCOUT_STATE_TYPES__
#define __SCOUT_STATE_TYPES__
#include <Arduino.h>

typedef struct {
    time_t start_time = 0;
    time_t gps_read_time = 0;
    // time_t expected_wakeup = 0;
    // uint32_t uptime = 0;
    uint32_t prior_uptime = 0;
    uint32_t frequency = 120;
    bool go_to_sleep = 0;
    bool gps_done = 0;
    bool message_sent = 0;
    bool display_off; // TODO: remove
    bool rockblock_done = 0;
    bool send_success; // TODO: remove
    float lat=999;
    float lng=999;
    float heading=0;
    float speed=0; // speed in knots
    float bat=0;
    int32_t rssi;
    char message[255] = {0};
} systemState;

#endif