/*
 * Small helpers removed from main for testing
 */

#include <Arduino.h>
#include <stateType.h>

/*
 * Calculate wake up time, pegging it to a time raster starting at the full
 * hour.
 */
time_t getNextWakeupTime(time_t now, uint16_t delay) {
    time_t full_hour = int(now/3600) * 3600;
    uint16_t cycles = int(now % 3600/delay);
    return full_hour + (cycles + 1) * delay;
}

 /*
 * Get sleep time in seconds from state
 * TODO: Remove side effects
 */
uint16_t getSleepDifference(systemState &state) {
    // make sure we count from start time since the time can pass while the
    // program is running
    time_t wakeUp = getNextWakeupTime(state.gps_read_time, state.frequency);
    int32_t difference = wakeUp - state.gps_read_time;
    // make sure difference is positive and we have some time to sleep
    if (!state.send_success && state.retries > 0) {
      if (state.retry_time < difference - state.timeout - state.retry_time - 61) {
        difference = state.retry_time;
      } else {
        state.retries = 3;
      }
    }
    /* Serial.print("start: " ); Serial.println(state.start_time);
    Serial.print("frequency: "); Serial.println(state.frequency);
    Serial.print("reference time: "); Serial.println( state.gps_read_time );
    Serial.print("wakeup time: "); Serial.println(wakeUp);
    Serial.print("difference: "); Serial.println(difference);
    Serial.print("retries left: "); Serial.println(state.retries);
    Serial.println(); */
    // set minimum sleep time, to ensure we wake up
    difference = ( difference < 5 ) ? 5 : difference;
    difference = ( difference > 259200 ) ? 259200 : difference;
    return difference;
  }