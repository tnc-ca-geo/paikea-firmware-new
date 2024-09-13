/*
 * The system state holds variables that are used by different components of the
 * system and shared between tasks. For this reason there are mutexes on the
 * setters and getters (TODO: check whether we need mutexes on getters.)
 */
#include <state.h>
#define WAIT pdMS_TO_TICKS(300)


SystemState::SystemState() {}


/*
 * Calculate next send time from current time.
 */
time_t SystemState::next_send_time(time_t now, uint16_t delay) {
    time_t full_hour = int(now/3600) * 3600;
    uint16_t cycles = int(now % 3600/delay);
    return full_hour + (cycles + 1) * delay;
}


/*
 * Initialize state from stored values.
 */
bool SystemState::init() {
    if (xSemaphoreTake( this->mutex, WAIT ) == pdTRUE) {
        if (rtc_first_run) {
            preferences.begin("scout", false);
            rtc_prior_uptime = preferences.getUInt("uptime", 0);
            preferences.end();
            Serial.print("Last uptime: ");
            Serial.println((uint32_t) rtc_prior_uptime);
            rtc_expected_wakeup = 0;
        }
        state.real_time = rtc_expected_wakeup;
        state.time_read_system_time = esp_timer_get_time()/1E6;
        xSemaphoreGive( this->mutex );
        return true;
    } else return false;
}

time_t SystemState::getTimeValue(time_t *ref) {
    time_t ret = 0;
    if (xSemaphoreTake( this->mutex, WAIT ) == pdTRUE) {
        ret = *ref;
        xSemaphoreGive( this->mutex );
        return ret;
    } else {
        Serial.println("BLOCKED");
        return 0;
    }
}

bool SystemState::setTimeValue(time_t *ref, time_t value) {
    if ( xSemaphoreTake( this->mutex, WAIT ) == pdTRUE ) {
        *ref = value;
        xSemaphoreGive( this->mutex );
        return true;
    } else return false;
}

size_t SystemState::getBuffer(char *ref, char *outBfr) {
    if (xSemaphoreTake( this->mutex, WAIT ) == pdTRUE) {
        const size_t size = strlen(ref);
        memcpy(outBfr, ref, size);
        xSemaphoreGive( this->mutex );
        outBfr[size] = 0;
        return size;
    }
    return 0;
}

bool SystemState::setBuffer(char *ref, char *inBfr) {
    const size_t size = strlen(inBfr);
    if (xSemaphoreTake( this->mutex, WAIT ) == pdTRUE) {
        memcpy(ref, inBfr, size);
        ref[size] = 0;
        xSemaphoreGive( this->mutex );
        return true;
    } else return false;
}

void SystemState::setBlinkSleepReady(bool val) { state.blink_sleep_ready = val; }
bool SystemState::getBlinkSleepReady() { return state.blink_sleep_ready; }

void SystemState::setDisplayOff(bool val) { state.display_off = val; }
bool SystemState::getDisplayOff() { return state.display_off; }

void SystemState::setGoToSleep(bool val) { state.go_to_sleep = val; }
bool SystemState::getGoToSleep() { return state.go_to_sleep; }

void SystemState::setGpsDone(bool val) { state.gps_done = val; }
bool SystemState::getGpsDone() { return state.gps_done; }

void SystemState::setRockblockDone(bool val) { state.rockblock_done = val; }
bool SystemState::getRockblockDone() { return state.rockblock_done; }

void SystemState::setMessageSent(bool val) { state.message_sent = val; }
bool SystemState::getMessageSent() { return state.message_sent; }

time_t SystemState::getRealTime() {
    return this->getTimeValue( &state.real_time );
}

time_t SystemState::getFrequency() { return rtc_frequency; }

time_t SystemState::getNextSendTime() {
    return next_send_time( this->getRealTime(), rtc_frequency );
};

// should be atomic
void SystemState::setLatitude(float value) { state.lat = value; }
float SystemState::getLatitude() { return state.lat; }
// should be atomic
void SystemState::setLongitude(float value) { state.lng = value; }
float SystemState::getLongitude() { return state.lng; }

/*
 * Calculate time from available information, we starting at epoch 0,
 * 1970-01-01 00:00:00 if we don't have any other information.
 */
bool SystemState::sync() {
    time_t set_time = esp_timer_get_time()/1E6;
    time_t rl_time = getTimeValue( &state.real_time );
    time_t rd_time = getTimeValue( &state.time_read_system_time );
    time_t time = rl_time + set_time - rd_time;
    state.uptime = rl_time - rtc_start;
    return (this->setTimeValue(&state.real_time, time)
        && this->setTimeValue(&state.time_read_system_time, set_time));
}

/*
 * Sync all time values with new information. Recalculate start time, when
 * real time is available.
 */
bool SystemState::setRealTime(time_t time, bool gps) {
    this->setTimeValue(
        &state.time_read_system_time, esp_timer_get_time()/1E6 );
    if (gps && rtc_first_fix) {
        rtc_start = time - esp_timer_get_time()/1E6;
        rtc_first_fix = false;
    }
    // setting real time to new value and sync all other time values
    return this->setTimeValue( &state.real_time, time ) && this->sync();
}

uint32_t SystemState::getUptime() { return state.uptime; }

void SystemState::setRssi(int32_t val) { state.rssi = val; }
int32_t SystemState::getRssi() { return state.expander_sleep_ready; }

bool SystemState::setMessage(char *bfr) {
    return this->setBuffer(state.message, bfr);
}

size_t SystemState::getMessage(char *bfr) {
    return this->getBuffer(state.message, bfr);
}

bool SystemState::getSystemSleepReady() {
    return state.blink_sleep_ready && state.gps_done && state.rockblock_done;
}

time_t SystemState::getPriorUptime() { return rtc_prior_uptime; }

time_t SystemState::getWakeupTime() { return rtc_expected_wakeup; };

/*
 * Write variables to peristent storage using preferences.h. Since it uses
 * the LittleFS filesystem, number of writes to SRAM is managed.
 */
 void SystemState::persist() {
    rtc_expected_wakeup = next_send_time(
        getTimeValue( &state.real_time), rtc_frequency );
    rtc_first_run = false;
    preferences.begin("scout", false);
    preferences.putUInt("uptime", state.uptime);
    preferences.end();
}
