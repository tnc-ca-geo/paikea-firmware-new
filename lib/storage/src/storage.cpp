#include <storage.h>


ScoutStorage::ScoutStorage() {}


void ScoutStorage::restore(systemState &state) {
    if (rtc_first_run) {
        preferences.begin("scout", false);
        state.prior_uptime = preferences.getUInt("uptime", 0);
        rtc_prior_uptime = state.prior_uptime;
        preferences.end();
        state.frequency = rtc_frequency;
        state.first_fix = rtc_first_fix;
        Serial.print("Last uptime: ");
        Serial.println((uint32_t) rtc_prior_uptime);
        rtc_expected_wakeup = 0;
    } else {
        state.real_time = rtc_expected_wakeup;
        state.time_read_system_time = esp_timer_get_time()/1E6;
        state.prior_uptime = rtc_prior_uptime;
        state.start_time = rtc_start;
    }
}


void ScoutStorage::store(systemState &state) {
    rtc_first_run = false;
    rtc_first_fix = state.first_fix;
    rtc_start = state.start_time;
    rtc_expected_wakeup = state.expected_wakeup;
    preferences.begin("scout", false);
    preferences.putUInt("uptime", state.uptime);
    preferences.end();
}