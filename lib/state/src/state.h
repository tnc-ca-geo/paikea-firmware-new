/*
 * Break out the state object in order to handle the mutex through setters and
 * getters. This could be a Singleton but that might make it more complicated
 * for now. This approach also has the advantage that we can create methods
 * that give us some combined logic of particular states.
 *
 * We need/use three levels of variable persistence:
 *
 * 1. Memory that only persists when up between two sleep cycles.
 * 2. Memory that persists through sleep cycles (RTC).
 *    - use slow RTC storage
 * 3. Storage that persists even if device is off (Preferences).
 *   - use preferences module
 */
#ifndef __STATE_H__
#define __STATE_H__

#include <Arduino.h>
#include <Preferences.h>

/*
 * This variables will be persisted during deep sleep using slow RTC memory
 * but cleared upon reset.
 */
inline RTC_DATA_ATTR time_t rtc_start = 0;
inline RTC_DATA_ATTR uint64_t rtc_expected_wakeup = 0;
inline RTC_DATA_ATTR time_t rtc_prior_uptime = 0;
inline RTC_DATA_ATTR time_t rtc_frequency = 300;
inline RTC_DATA_ATTR bool rtc_first_fix = true;
inline RTC_DATA_ATTR bool rtc_first_run = true;


class SystemState {

    private:
        SemaphoreHandle_t mutex=xSemaphoreCreateMutex();
        /*
         * Use Preferences for persistent data storage and debugging when fully
         * powered off. The Preferences library manages the limited number of
         * write cycles for this kind of storage.
         */
        Preferences preferences;
        time_t real_time = 0;
        uint32_t prior_uptime = 0;
        // use uint here since it is a time difference
        uint32_t uptime = 0;
        // system time when time was last updated from a real time reading
        time_t time_read_system_time = 0;
        // uint16_t frequency = 300;
        bool go_to_sleep = 0;
        bool gps_done = 0;
        bool message_sent = 0;
        bool blink_sleep_ready; // TODO: remove
        bool expander_sleep_ready; // TODO: remove
        bool display_off; // TODO: remove
        bool rockblock_done = 0;
        bool send_success; // TODO: remove
        // Setting no value into coordinates, to indicate if GPS is not working.
        float lat=999;
        float lng=999;
        int32_t rssi;
        char message[255] = {0};
        // private methods
        time_t next_send_time(time_t now, uint16_t delay);
        time_t getTimeValue(time_t *ref);
        bool setTimeValue(time_t *ref, time_t value);
        size_t getBuffer(char *ref, char *outBfr);
        bool setBuffer(char *ref, char *inBfr);

    public:
        SystemState();
        // init populates state variables from persistent storage
        bool init();
        bool getBlinkSleepReady();
        void setBlinkSleepReady(bool val);
        bool getDisplayOff();
        void setDisplayOff(bool val);
        bool getGoToSleep();
        void setGoToSleep(bool val);
        bool getGpsDone();
        void setGpsDone(bool val);
        size_t getMessage(char *bfr);
        bool setMessage(char *bfr);
        void setRockblockDone(bool val);
        bool getRockblockDone();
        void setMessageSent(bool val);
        bool getMessageSent();
        bool getSystemSleepReady();
        void setLongitude(float value);
        float getLongitude();
        void setLatitude(float value);
        float getLatitude();
        void setRssi(int32_t val);
        int32_t getRssi();
        time_t getPriorUptime();
        time_t getWakeupTime();
        time_t getFrequency();
        time_t getRealTime();
        time_t getNextSendTime();
        // sync with new actual time information
        bool setRealTime(time_t time, bool gps=false);
        // sync all time values, implicitely called by setRealTime()
        bool sync();
        uint32_t getUptime();
        // write variables that should survive sleep or power off
        void persist();
};

#endif /* __STATE_H__ */