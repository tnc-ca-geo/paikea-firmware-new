#include <state.h>
#define WAIT pdMS_TO_TICKS(50)


SystemState::SystemState() {}

// Find SystemState::init below since it correspondents with ::persist


bool SystemState::getBoolValue(bool *ref) {
    bool ret = false;
    if ( xSemaphoreTake( this->mutex, WAIT ) == pdTRUE ) {
        ret = *ref;
        xSemaphoreGive( this->mutex );
        return ret;
    } else return false;
}


bool SystemState::setBoolValue(bool *ref, bool value) {
    if ( xSemaphoreTake( this->mutex, WAIT ) == pdTRUE ) {
        *ref = value;
        xSemaphoreGive( this->mutex );
        return true;
    } else return false;
}


uint64_t SystemState::getTimeValue(uint64_t *ref) {
    uint64_t ret = 0;
    if (xSemaphoreTake( this->mutex, WAIT ) == pdTRUE) {
        ret = *ref;
        xSemaphoreGive( this->mutex );
        return ret;
    } else return 0;
}


bool SystemState::setTimeValue(uint64_t *ref, uint64_t value) {
    if ( xSemaphoreTake( this->mutex, WAIT ) == pdTRUE ) {
        *ref = value;
        xSemaphoreGive( this->mutex );
        return true;
    } else return false;
}


int32_t SystemState::getIntegerValue(int32_t *ref) {
    int32_t ret = 0;
    if (xSemaphoreTake( this->mutex, WAIT ) == pdTRUE) {
        ret = *ref;
        xSemaphoreGive( this->mutex );
        return ret;
    } else return 0;
}


bool SystemState::setIntegerValue(int32_t *ref, int32_t value) {
    if ( xSemaphoreTake( this->mutex, WAIT ) == pdTRUE ) {
        *ref = value;
        xSemaphoreGive( this->mutex );
        return true;
    } else return false;
}


size_t SystemState::getBuffer(char *ref, char *outBfr) {
    if (xSemaphoreTake( this->mutex, WAIT ) == pdTRUE) {
        const size_t size = strlen(ref);
        // Serial.print("Size "); Serial.println(size);
        memcpy(outBfr, ref, size);
        xSemaphoreGive( this->mutex );
        outBfr[size] = 0;
        // Serial.print("OUT "); Serial.print(outBfr); Serial.println();
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


bool SystemState::getBlinkSleepReady() {
    return this->getBoolValue( &this->blink_sleep_ready );
}


bool SystemState::setBlinkSleepReady(bool value) {
    return this->setBoolValue( &this->blink_sleep_ready, value );
}


bool SystemState::getDisplayOff() {
    return this->getBoolValue( &this->display_off );
}


bool SystemState::setDisplayOff(bool value) {
    return this->setBoolValue( &this->display_off, value );
}


bool SystemState::getGoToSleep() {
    return this->getBoolValue( &this->go_to_sleep );
}


bool SystemState::setGoToSleep(bool value) {
    return this->setBoolValue( &this->go_to_sleep, value );
}


bool SystemState::getGpsDone() {
    return this->getBoolValue( &this->gps_done );
}


bool SystemState::setGpsDone(bool value) {
    return this->setBoolValue( &this->gps_done, value );
}


bool SystemState::getGpsGotFix() {
    return this->getBoolValue( &this->gps_got_fix );
}


bool SystemState::setGpsGotFix(bool value) {
    return this->setBoolValue( &this->gps_got_fix, value );
}


bool SystemState::getRockblockSleepReady() {
    return this->getBoolValue( &this->rockblock_sleep_ready );
}


bool SystemState::setRockblockSleepReady(bool value) {
    return this->setBoolValue( &this->rockblock_sleep_ready, value );
}


uint64_t SystemState::getRealTime() {
    return this->getTimeValue( &this->real_time );
}


bool SystemState::setRealTime(uint64_t time) {
    return this->setTimeValue( &this->real_time, time );
}


uint64_t SystemState::getUptime() {
    return this->getTimeValue( &this->uptime );
}


bool SystemState::setUptime(uint64_t time) {
    return this->setTimeValue( &this->uptime, time );
}

int32_t SystemState::getRssi() {
    return this->getIntegerValue( &this->rssi );
}


bool SystemState::setRssi(int32_t value) {
    return this->setIntegerValue( &this->rssi, value);
}


size_t SystemState::getMessage(char *bfr) {
    return this->getBuffer(this->message, bfr);
}


bool SystemState::setMessage(char *bfr) {
    return this->setBuffer(this->message, bfr);
}

/*
 * Initialize state from stored values, currently uptime is read into rtc
 * memory that is not managed by this class.
 *
 * TODO: Clean up. Load all vars into state during runtime.
 */
bool SystemState::init() {
    if (xSemaphoreTake( this->mutex, WAIT ) == pdTRUE) {
        preferences.begin("debug", false);
        // add variables to restore
        preferences.end();
        xSemaphoreGive( this->mutex );
        return true;
    } else return false;
}

/*
 * Write variables to peristent storage using preferences.h. Since it uses
 * the LittleFS filesystem, number of writes to SRAM is managed.
 */
bool SystemState::persist() {
    if (xSemaphoreTake( this->mutex, WAIT ) == pdTRUE) {
        preferences.begin("debug", false);
        // add all variables that need to be persisted here, use sparengly
        preferences.putUInt("uptime", this->uptime);
        preferences.end();
        xSemaphoreGive( this->mutex );
        return true;
    } else return false;
}