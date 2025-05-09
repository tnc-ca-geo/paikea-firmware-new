#include <helpers.h>

#ifndef SYSTEM_TIME_OUT
#define SYSTEM_TIME_OUT 300
#endif

/*
 * Calculate wake up time, pegging it to actual time starting at 00:00:00.
 */
time_t helpers::getNextWakeupTime(time_t now, unsigned int delay) {
  time_t full_day = int(now/86400) * 86400;
  uint16_t cycles = int(now % 86400/delay);
  return full_day + (cycles + 1) * delay;
}

/*
 * Print epoch as time.
 */
void helpers::printTime(const time_t time) {
  char bfr[32] = {0};
  strftime(bfr, 32, "%T", gmtime(&time));
  Serial.print(bfr);
}

/*
 * Get sleep time and retries from state. Side effects: state.mode will be
 * corrected to NORMAL if we there is no time for retry.
*/
uint32_t helpers::getSleepDifference(systemState &state, const time_t now) {

  // Make sure that the new time is not the same as the old time
  time_t reference = (state.gps_read_time > state.expected_wakeup) ?
    state.gps_read_time : state.expected_wakeup + 1;

  // Calculate time for wakeup, retry, and sleep
  time_t wakeUp = getNextWakeupTime(reference, state.interval);
  time_t retryWakeup = getNextWakeupTime(reference, RETRY_INTERVAL);

  // Set mode to normal if retry time is over
  if ( state.mode == RETRY ) {
    if (retryWakeup + SYSTEM_TIME_OUT < wakeUp) { wakeUp = retryWakeup; }
    // no time for retries left
    else {
      state.retries = 3;
      state.mode = NORMAL;
    }
  }

  // We still have to calculate from now, this could be potentially negative
  // and will be corrected below
  int32_t difference = (state.mode == CONFIG) ? MINIMUM_SLEEP : wakeUp - now;
  // Sleep time takes precidence
  if (state.sleep != 0) { difference = state.sleep; }

  /* Serial.print("interval: "); Serial.println(state.interval);
  Serial.print("retry time: "); Serial.println(RETRY_INTERVAL);
  Serial.print("\nreference time (gps): "); printTime( reference );
  Serial.print("\nnow: "); printTime(now);
  Serial.print("\nwakeup time: "); printTime(wakeUp);
  Serial.print("\ndifference: "); Serial.println(difference);
  Serial.print("retries left: "); Serial.println(state.retries);
  Serial.println(); */

  // set minimum sleep time, to ensure we wake up
  difference = ( difference < MINIMUM_SLEEP ) ? MINIMUM_SLEEP : difference;
  // Sleep time is 3 days maximum
  difference = ( difference > MAXIMUM_SLEEP ) ? MAXIMUM_SLEEP : difference;

  state.expected_wakeup = now + difference;

  return difference;
}

/*
 * Update state after send success and from incoming message bfr (or timeout)
 * :param systemState state pointer: A pointer to the system state object
 * :param char bfr pointer: A pointer to the message bfr
 * :param time_t runtime: Time since wakeup or system start
 * :param bool success: Send success
 * :param bool busy: RB still busy
 * :return type fsmState
 */
mainFSM helpers::update_state_from_rb_msg(
    systemState &state, char *bfr, time_t runtime, bool success=false,
    bool busy=true
) {
    // Success and done retrieving incoming message
    if (success && !busy) {
      state.mode = state.gps_done ? NORMAL : RETRY;
      // apply new interval
      if (state.new_interval != 0) {
        state.interval = state.new_interval;
        state.new_interval = 0;
      }
      if (state.new_sleep != 0) {
        state.sleep = state.new_sleep;
        state.new_sleep = 0;
      }
      if (bfr[0] != '\0' &&  scoutMessages::parseIncoming(state, bfr) ) {
        if ( state.new_interval != state.interval) {
          state.mode = CONFIG;
          state.interval = 600;
        }
        if (state.new_sleep != 0) {
          state.mode = CONFIG;
          state.interval = 600;
        }
      }
      state.retries = 3;
    }

    // Timeout
    else if (runtime > SYSTEM_TIME_OUT && !busy) { state.mode = RETRY; }

    // Keep polling
    else { return WAIT_FOR_RB; }

    // check whether retries left
    if (state.mode == RETRY) {
        if (state.retries > 0) {
          state.retries--;
        } else {
          state.retries = 3;
          state.mode = NORMAL;
        }
    }

    return SLEEP_READY;
}

/*
 * Update from GPS (side effect) and return next state
 * :param systemState state: A pointer to the system state
 * :param Gps gps: A pointer to the gps class
 * :param time_t time: Time when function called
 * :param bool timeout: Whether to timeout the GPS
 * :return type mainFSM:
 */
mainFSM helpers::update_state_from_gps(
    systemState &state, Gps &gps, time_t time, bool timeout=false
) {
    if (!gps.updated && !timeout) { return WAIT_FOR_GPS; }
    state.lat = gps.lat;
    state.lng = gps.lng;
    state.heading = gps.heading;
    state.speed = gps.speed;
    state.gps_done = true;
    state.gps_read_time = gps.get_corrected_epoch();
    if (state.start_time == 0) {
      state.start_time = gps.get_corrected_epoch() - time;
    }
    return WAIT_FOR_RB;
}
