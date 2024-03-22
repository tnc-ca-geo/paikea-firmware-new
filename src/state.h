#ifndef __STATE
#define __STATE
/*
 * Break out the state object in order to handle the mutex through setters and
 * getters. This could be a Singleton but that might make it more complicated
 * for now.
 */

class State {

    private:
        time_t real_time;
        time_t prior_uptime;
        time_t uptime;
        // We inform all components to go to sleep gracefully and check whether
        // they are ready
        bool go_to_sleep;
        bool gps_done;
        bool gps_got_fix;
        bool blink_sleep_ready;
        bool expander_sleep_ready;
        bool display_off;
        bool rockblock_sleep_ready;
        bool send_success;
        int16_t rssi;
        char message[255] = {0};
    public:
        time_t getRealTime();
}

#endif