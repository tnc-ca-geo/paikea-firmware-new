#include <storage.h>


ScoutStorage::ScoutStorage() {}


void ScoutStorage::restore(systemState &state) {
    if (rtc_first_run) {
        preferences.begin("scout", false);
        rtc_prior_uptime = preferences.getUInt("uptime", 0);
        preferences.end();
        state.frequency = rtc_frequency;
        Serial.print("Last uptime: ");
        Serial.println((uint32_t) rtc_prior_uptime);
        // rtc_expected_wakeup = 0;
    } else {
        // state.time_read_system_time = esp_timer_get_time()/1E6;
        state.prior_uptime = rtc_prior_uptime;
        state.start_time = rtc_start;
    }
}


void ScoutStorage::store(systemState &state) {
    rtc_first_run = false;
    rtc_start = state.start_time;
    // rtc_expected_wakeup = state.expected_wakeup;
    preferences.begin("scout", false);
    // preferences.putUInt("uptime", state.uptime);
    preferences.end();
}