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
        uint64_t real_time = 0;
        uint64_t prior_uptime = 0;
        uint64_t uptime = 0;
        // system time when time was last updated from a real time reading
        uint64_t time_read_system_time = 0;
        // uint16_t frequency = 300;
        bool go_to_sleep =0;
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
        uint64_t next_send_time(uint64_t now, uint16_t delay);
        bool getBoolValue(bool *ref);
        bool setBoolValue(bool *ref, bool value);
        uint64_t getTimeValue(uint64_t *ref);
        bool setTimeValue(uint64_t *ref, uint64_t value);
        bool setIntegerValue(int32_t *ref, int32_t value);
        size_t getBuffer(char *ref, char *outBfr);
        bool setBuffer(char *ref, char *inBfr);

    public:
        SystemState();
        // init populates state variables from persistent storage
        bool init();
        bool getBlinkSleepReady();
        bool setBlinkSleepReady(bool value);
        bool getDisplayOff();
        bool setDisplayOff(bool value);
        bool getGoToSleep();
        bool setGoToSleep(bool value);
        bool getGpsDone();
        bool setGpsDone(bool value);
        size_t getMessage(char *bfr);
        bool setMessage(char *bfr);
        bool getRockblockDone();
        bool setRockblockDone(bool value);
        bool getSystemSleepReady();
        bool getMessageSent();
        bool setMessageSent(bool value);
        void setLongitude(float value);
        float getLongitude();
        void setLatitude(float value);
        float getLatitude();
        int32_t getRssi();
        bool setRssi(int32_t value);
        uint64_t getPriorUptime();
        uint64_t getWakeupTime();
        uint64_t getFrequency();
        uint64_t getRealTime();
        uint64_t getNextSendTime();
        // sync with new actual time information
        bool setRealTime(uint64_t time, bool gps=false);
        // sync all time values, implicitely called by setRealTime()
        bool sync();
        uint64_t getUptime();
        // bool setUptime(uint64_t time);
        // write variables that should survive sleep or power off
        bool persist();
};

#endif /* __STATE_H__ */