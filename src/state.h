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


class SystemState {

    private:
        SemaphoreHandle_t mutex=xSemaphoreCreateMutex();
        /*
         * Use Preferences for persistent data storage and debugging when fully
         * powered off. The Preferences library supposedly manages limited
         * number of write write cycles to this kind of storage. Which is
         * certainly not an issue with Scout but might be an issue with the
         * Glue devices. So don't relay on this too much.
         */
        Preferences preferences;
        uint64_t real_time;
        uint64_t prior_uptime;
        uint64_t uptime;
        bool go_to_sleep;
        bool gps_done;
        bool gps_got_fix;
        bool blink_sleep_ready;
        bool expander_sleep_ready;
        bool display_off;
        bool rockblock_sleep_ready;
        bool send_success;
        int32_t rssi;
        char message[255] = {0};
        // methods
        bool getBoolValue(bool *ref);
        bool setBoolValue(bool *ref, bool value);
        uint64_t getTimeValue(uint64_t *ref);
        bool setTimeValue(uint64_t *ref, uint64_t value);
        int32_t getIntegerValue(int32_t *ref);
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
        bool getGpsGotFix();
        bool setGpsGotFix(bool value);
        size_t getMessage(char *bfr);
        bool setMessage(char *bfr);
        bool getRockblockSleepReady();
        bool setRockblockSleepReady(bool value);
        int32_t getRssi();
        bool setRssi(int32_t value);
        uint64_t getPriorUptime();
        bool setPriorUptime(uint64_t time);
        uint64_t getRealTime();
        bool setRealTime(uint64_t time);
        uint64_t getUptime();
        bool setUptime(uint64_t time);
        // sleep writes some state variables into persistent storage
        bool persist();
};

#endif /* __STATE_H__ */