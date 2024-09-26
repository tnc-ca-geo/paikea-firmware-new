/*
 * Persist and restore values in deep sleep (RTC storage) or turned off
 * (Preferences).
 */
#ifndef __SCOUT_STORAGE__
#define __SCOUT_STORAGE__
#include <Preferences.h>
#include <stateType.h>


inline RTC_DATA_ATTR time_t rtc_start = 0;
// inline RTC_DATA_ATTR time_t rtc_expected_wakeup = 0;
inline RTC_DATA_ATTR time_t rtc_prior_uptime = 0;
inline RTC_DATA_ATTR uint32_t rtc_frequency = 120;
// inline RTC_DATA_ATTR bool rtc_first_fix = true;
inline RTC_DATA_ATTR bool rtc_first_run = true;


class ScoutStorage {

    private:
        Preferences preferences;

    public:
        ScoutStorage();
        void restore(systemState &state);
        void store(systemState &state);

};

#endif