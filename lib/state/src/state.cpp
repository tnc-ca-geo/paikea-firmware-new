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
            rtc_first_run = false;
            Serial.print("Last uptime: ");
            Serial.println((uint32_t) rtc_prior_uptime);
            rtc_expected_wakeup = 0;
        }
        this->real_time = rtc_expected_wakeup;
        this->time_read_system_time = esp_timer_get_time()/1E6;
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

void SystemState::setBlinkSleepReady(bool val) { blink_sleep_ready = val; }
bool SystemState::getBlinkSleepReady() { return blink_sleep_ready; }

void SystemState::setDisplayOff(bool val) { display_off = val; }
bool SystemState::getDisplayOff() { return display_off; }

void SystemState::setGoToSleep(bool val) { go_to_sleep = val; }
bool SystemState::getGoToSleep() { return go_to_sleep; }

void SystemState::setGpsDone(bool val) { gps_done = val; }
bool SystemState::getGpsDone() { return gps_done; }

void SystemState::setRockblockDone(bool val) { rockblock_done = val; }
bool SystemState::getRockblockDone() { return rockblock_done; }

void SystemState::setMessageSent(bool val) { message_sent = val; }
bool SystemState::getMessageSent() { return message_sent; }

time_t SystemState::getRealTime() {
    return this->getTimeValue( &this->real_time );
}

time_t SystemState::getFrequency() { return rtc_frequency; }

time_t SystemState::getNextSendTime() {
    return next_send_time( this->getRealTime(), rtc_frequency );
};

// should be atomic
void SystemState::setLatitude(float value) { this->lat = value; }
float SystemState::getLatitude() { return this->lat; }
// should be atomic
void SystemState::setLongitude(float value) { this->lng = value; }
float SystemState::getLongitude() { return this->lng; }

/*
 * Calculate time from available information, we starting at epoch 0,
 * 1970-01-01 00:00:00 if we don't have any other information.
 */
bool SystemState::sync() {
    time_t set_time = esp_timer_get_time()/1E6;
    time_t rl_time = getTimeValue( &this->real_time );
    time_t rd_time = getTimeValue( &this->time_read_system_time );
    time_t time = rl_time + set_time - rd_time;
    uptime = rl_time - rtc_start;
    return (this->setTimeValue(&this->real_time, time)
        && this->setTimeValue(&this->time_read_system_time, set_time));
}

/*
 * Sync all time values with new information. Recalculate start time, when
 * real time is available.
 */
bool SystemState::setRealTime(time_t time, bool gps) {
    this->setTimeValue( &this->time_read_system_time, esp_timer_get_time()/1E6);
    if (gps && rtc_first_fix) {
        rtc_start = time - esp_timer_get_time()/1E6;
        rtc_first_fix = false;
    }
    // setting real time to new value and sync all other time values
    return this->setTimeValue( &this->real_time, time ) && this->sync();
}

uint32_t SystemState::getUptime() { return this->uptime; }

void SystemState::setRssi(int32_t val) { rssi = val; }
int32_t SystemState::getRssi() { return this->rssi; }

bool SystemState::setMessage(char *bfr) {
    return this->setBuffer(this->message, bfr);
}

size_t SystemState::getMessage(char *bfr) {
    return this->getBuffer(this->message, bfr);
}

bool SystemState::getSystemSleepReady() {
    return this->blink_sleep_ready && this->gps_done && this->rockblock_done;
}

time_t SystemState::getPriorUptime() { return rtc_prior_uptime; }

time_t SystemState::getWakeupTime() { return rtc_expected_wakeup; };

/*
 * Write variables to peristent storage using preferences.h. Since it uses
 * the LittleFS filesystem, number of writes to SRAM is managed.
 */
 void SystemState::persist() {
    rtc_expected_wakeup = next_send_time(
        getTimeValue( &this->real_time), rtc_frequency );
    preferences.begin("scout", false);
    preferences.putUInt("uptime", uptime);
    preferences.end();
}
