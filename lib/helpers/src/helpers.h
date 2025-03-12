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

void printTime(const time_t time) {
  char bfr[32] = {0};
  strftime(bfr, 32, "%T", gmtime(&time));
  Serial.print(bfr);
}

 /*
 * Get sleep time in seconds from state
 * TODO: Remove side effects
 */
uint16_t getSleepDifference(systemState &state, time_t now) {
    // We are counting from the last GPS read time since we might miss the
    // the wakeUp time while trying to send.
    time_t wakeUp = getNextWakeupTime(state.gps_read_time, state.frequency);
    time_t retryWakeup = getNextWakeupTime(
      state.gps_read_time, state.retry_time);
    // We sill have to calculate from now, this could be potentially negative
    // and will be corrected below
    int32_t difference = wakeUp - now;
    // make sure difference is positive and we have some time to sleep
    if (!state.send_success && state.retries > 0) {
      if (retryWakeup < wakeUp) { wakeUp = retryWakeup; }
      else { state.retries = 3; }
    }

    Serial.print("frequency: "); Serial.println(state.frequency);
    Serial.print("retry time: "); Serial.println(state.retry_time);
    Serial.print("start time: " ); printTime(state.start_time);
    Serial.print("\nreference time (gps): "); printTime( state.gps_read_time );
    Serial.print("\nnow: "); printTime(now);
    Serial.print("\nwakeup time: "); printTime(wakeUp);
    Serial.print("\ndifference: "); Serial.println(difference);
    Serial.print("retries left: "); Serial.println(state.retries);
    Serial.println();
    // set minimum sleep time, to ensure we wake up
    difference = ( difference < 10 ) ? 10 : difference;
    // Sleep time is 3 days maximum
    difference = ( difference > 259200 ) ? 259200 : difference;
    return difference;
  }