#include <storage.h>

// treat RTC storage as local vars
RTC_DATA_ATTR time_t rtc_start = 0;
RTC_DATA_ATTR time_t rtc_expected_wakeup = 0;
RTC_DATA_ATTR unsigned int rtc_interval = 600;
RTC_DATA_ATTR bool rtc_first_run = true;
RTC_DATA_ATTR unsigned int rtc_retries = 3;
RTC_DATA_ATTR unsigned int rtc_new_interval = 0;
RTC_DATA_ATTR unsigned int rtc_sleep = 0;
RTC_DATA_ATTR messageType rtc_mode = NORMAL;


ScoutStorage::ScoutStorage() {}


void ScoutStorage::restore(systemState &state) {
    if (rtc_first_run) {
        // retrieve variables that would survive power down
        preferences.begin("scout", false);
        preferences.end();
        state.mode = FIRST;
        state.first_run = true;
    } else {
        // restore other variables, there are non during the first run
        state.first_run = false;
        state.start_time = rtc_start;
        state.expected_wakeup = rtc_expected_wakeup;
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
    if (rtc_start == 0) { rtc_start = state.start_time; }
    rtc_expected_wakeup = state.expected_wakeup;
    rtc_interval = state.interval;
    rtc_new_interval = state.new_interval;
    rtc_sleep = state.new_sleep;
    rtc_retries = state.retries;
    rtc_mode = state.mode;
    // store variables that should persisted even after power down
    preferences.begin("scout", false);
    preferences.end();
}