/*
 * Define state type for better testibility
 */
#ifndef __SCOUT_STATE_TYPES__
#define __SCOUT_STATE_TYPES__

#ifndef DEFAULT_INTERVAL
#define DEFAULT_INTERVAL 600
#endif

/*
 * States that indicate where the system left when going to sleep, will also
 * transferred to the downstream uplications with message (basis of timing)
 */
enum messageType {
    UNKNOWN,
    FIRST,
    NORMAL,
    CONFIG,
    RETRY // a retry message could be a config message at the same time
};

/*
 * Define states for Main FSM
 */
enum mainFSM {
  AWAKE,
  WAIT_FOR_GPS,
  WAIT_FOR_RB,
  RB_DONE,
  SLEEP_READY
};

typedef struct {
    // timing
    time_t start_time = 0; // time when buoy firts powered on (inlcudes sleep times)
    time_t gps_read_time = 0; // time when GPS was read
    uint32_t interval = DEFAULT_INTERVAL; // reporting interval
    uint8_t retries = 3; // maximal number of retries
    messageType mode = UNKNOWN;
    // state
    bool gps_done = 0;
    bool message_sent = 0;
    bool rockblock_done = 0;
    bool send_success = false;
    float lat=999;
    float lng=999;
    float heading=0;
    float speed=0; // speed in knots
    float bat=0;
    uint8_t signal = 0;
    // message
    char message[255] = {0};
    // requested configuration change
    uint32_t new_interval = 0;
    // There is actually no preset sleep (would always be 0) but I still mark
    // it as new because it belongs in the same category of presets that have
    // to be stored while sleeping.
    uint32_t new_sleep = 0;
    bool config_change_requested = false;
} systemState;

#endif