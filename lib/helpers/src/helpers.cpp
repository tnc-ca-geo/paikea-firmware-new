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
* Get sleep time and retries from state
* TODO: Remove side effects
*/
uint32_t helpers::getSleepDifference(systemState &state, const time_t now) {

  // We are counting from the last GPS read time since we might miss the
  // the wakeUp time while trying to send. Use the start time if we don't
  // have a fix.
  time_t reference = (
    state.gps_read_time != 0) ? state.gps_read_time : state.start_time;

  // Calculate time for wakeup, retry, and sleep
  time_t wakeUp = getNextWakeupTime(reference, state.interval);
  time_t retryWakeup = getNextWakeupTime(reference, RETRY_INTERVAL);

  // Apply retries if we don't have send success or we are in config transition.
  // Use retry time if sooner than scheduled time.
  if (state.retries > 0 && !state.send_success || state.mode == CONFIG) {
    if (retryWakeup < wakeUp) { wakeUp = retryWakeup; }
    // reset retries if pick scheduled time
    else { state.retries = 3; }
  }

  // We sill have to calculate from now, this could be potentially negative
  // and will be corrected below
  int32_t difference = wakeUp - now;

  Serial.print("interval: "); Serial.println(state.interval);
  Serial.print("retry time: "); Serial.println(RETRY_INTERVAL);
  Serial.print("start time: " ); printTime(state.start_time);
  Serial.print("\nreference time (gps): "); printTime( reference );
  Serial.print("\nnow: "); printTime(now);
  Serial.print("\nwakeup time: "); printTime(wakeUp);
  Serial.print("\ndifference: "); Serial.println((int) difference);
  Serial.print("retries left: "); Serial.println(state.retries);
  Serial.println();

  // set minimum sleep time, to ensure we wake up
  difference = ( difference < MINIMUM_SLEEP ) ? MINIMUM_SLEEP : difference;
  // Sleep time is 3 days maximum
  difference = ( difference > MAXIMUM_SLEEP ) ? MAXIMUM_SLEEP: difference;

  // Minimum sleep if first try of a transition
  if (state.mode == CONFIG && state.retries == 3) {
    state.retries == 2;
    difference = MINIMUM_SLEEP;
  }
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
    state.mode = state.gps_done ? NORMAL : RETRY;

    // Success and done retrieving incoming message
    if (success && !busy) {
      if (bfr[0] != '\0' &&  scoutMessages::parseIncoming(state, bfr) ) {
        if ( state.new_interval != state.interval) {
            state.mode = CONFIG;
            state.interval = 600;
        }
      }
      state.retries = 3;
    }

    // Timeout
    else if (runtime > SYSTEM_TIME_OUT) { state.mode = RETRY; }

    // Keep polling
    else { return WAIT_FOR_RB; }

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
