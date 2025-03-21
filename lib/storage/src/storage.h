/*
 * Persist and restore values in deep sleep (RTC storage) or during power off
 * (using Preferences).
 */
#ifndef __SCOUT_STORAGE__
#define __SCOUT_STORAGE__

#include <Preferences.h>
#include <stateType.h>

inline RTC_DATA_ATTR time_t rtc_start = 0;
inline RTC_DATA_ATTR unsigned int rtc_interval = 600;
inline RTC_DATA_ATTR bool rtc_first_run = true;
inline RTC_DATA_ATTR unsigned int rtc_retries = 3;
inline RTC_DATA_ATTR unsigned int rtc_new_interval = 0;
inline RTC_DATA_ATTR unsigned int rtc_sleep = 0;
inline RTC_DATA_ATTR messageType rtc_mode = UNKNOWN;

/*
 * Store or restore some parts of the system state. Some aspects need to be
 * stored during sleep others might need to be stored even under power off
 * (currently not utilized.)
 *
 * IMPROVEMENT? We only store the state partially. Since it is a struct, we
 * might be able to store the entire state.
 */
class ScoutStorage {

    private:
        Preferences preferences;

    public:
        ScoutStorage();
        void restore(systemState &state);
        void store(systemState &state);
};

#endif
