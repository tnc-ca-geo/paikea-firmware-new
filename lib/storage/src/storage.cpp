#include <storage.h>


ScoutStorage::ScoutStorage() {}


void ScoutStorage::restore(systemState &state) {
    if (rtc_first_run) {
        // retrieve variables that would survive power down
        preferences.begin("scout", false);
        preferences.end();
    } else {
        // restore all the other variables, there are non-during the first run
        state.start_time = rtc_start;
        state.frequency = rtc_frequency;
        // state.retries = rtc_retries;
    }
}


void ScoutStorage::store(systemState &state) {
    // we persist state only while sleeping for now
    rtc_first_run = false;
    rtc_start = state.start_time;
    rtc_frequency = state.frequency;
    // rtc_retries = state.retries;
    // store variables that should persisted even after power down here
    preferences.begin("scout", false);
    preferences.end();
}