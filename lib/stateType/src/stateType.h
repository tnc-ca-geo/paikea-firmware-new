/*
 * Define the state type here for better testibility
 */
#ifndef __SCOUT_STATE_TYPES__
#define __SCOUT_STATE_TYPES__
#include <Arduino.h>

typedef struct {
    time_t real_time = 0;
    time_t start_time = 0;
    time_t time_read_system_time = 0;
    time_t expected_wakeup = 0;
    uint32_t uptime = 0;
    uint32_t prior_uptime = 0;
    uint32_t frequency = 120;
    bool first_fix = false;
    bool go_to_sleep = 0;
    bool gps_done = 0;
    bool message_sent = 0;
    bool blink_sleep_ready; // TODO: remove
    bool expander_sleep_ready; // TODO: remove
    bool display_off; // TODO: remove
    bool rockblock_done = 0;
    bool send_success; // TODO: remove
    float lat=999;
    float lng=999;
    int32_t rssi;
    char message[255] = {0};
} systemState;

#endif