#include <storage.h>


ScoutStorage::ScoutStorage() {}


void ScoutStorage::restore(systemState &state) {
    if (rtc_first_run) {
        // retrieve variables that would survive power down
        preferences.begin("scout", false);
        preferences.end();
        state.mode = FIRST;
    } else {
        // restore other variables, there are non during the first run
        state.mode = NORMAL;
        state.start_time = rtc_start;
        state.interval = rtc_interval;
        state.new_interval = rtc_new_interval;
        state.retries = rtc_retries;
        state.new_sleep = rtc_sleep;
        state.mode = rtc_mode;
    }
}


void ScoutStorage::store(systemState &state) {
    // we persist state only while sleeping for now
    rtc_first_run = false;
    rtc_start = state.start_time;
    rtc_interval = state.interval;
    rtc_new_interval = state.new_interval;
    rtc_sleep = state.new_sleep;
    rtc_retries = state.retries;
    rtc_mode = state.mode;
    // store variables that should persisted even after power down
    preferences.begin("scout", false);
    preferences.end();
}