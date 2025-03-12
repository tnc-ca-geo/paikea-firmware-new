/*
 * Define state type for better testibility
 */
#ifndef __SCOUT_STATE_TYPES__
#define __SCOUT_STATE_TYPES__


typedef struct {
    time_t start_time = 0;
    time_t gps_read_time = 0;
    // TODO: remove
    uint32_t prior_uptime = 0;
    uint32_t frequency = 600;
    uint8_t retries = 3;
    // TODO: remove
    bool go_to_sleep = 0;
    bool gps_done = 0;
    bool message_sent = 0;
    // make sure that is true otherwise application will hang in production mode
    // bool display_off = true;
    bool rockblock_done = 0;
    bool send_success = false;
    float lat=999;
    float lng=999;
    float heading=0;
    float speed=0; // speed in knots
    float bat=0;
    uint8_t signal = 0;
    // setting timeout to 300 seconds
    const uint16_t timeout = 600;
    // setting retry time to 120 seconds
    const uint16_t retry_time = 600;
    char message[255] = {0};
} systemState;

#endif