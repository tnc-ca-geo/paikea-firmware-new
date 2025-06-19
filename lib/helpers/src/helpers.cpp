#include <helpers.h>

#define SECS_IN_A_DAY 86400

/*
 * Calculate wake up time, pegging it to actual time starting at 00:00:00.
 * We are currently not allowing reporting intervals over 24 hours. This
 * function will return next midnight as a maximum.
 */
time_t helpers::getNextWakeupTime(time_t now, unsigned int delay) {
  time_t full_day = int(now/SECS_IN_A_DAY) * SECS_IN_A_DAY;
  uint16_t cycles = int(now % SECS_IN_A_DAY/delay);
  time_t time = full_day + (cycles + 1) * delay;
  if (time > full_day + SECS_IN_A_DAY) {
    return full_day + SECS_IN_A_DAY;
  } else {
    return time;
  }
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
  // Make sure that the new time is not the same as the old time, we substract
  // 1 from the alternative time to account for state.gps_read_time being
  // exactly on the time
  time_t reference = (state.gps_read_time > state.expected_wakeup) ?
    state.gps_read_time : state.gps_read_time + state.interval - 1;
  // Calculate times for wakeup, and retry
  time_t regularWakeup = getNextWakeupTime(reference, state.interval);
  time_t retryWakeup = getNextWakeupTime(reference, RETRY_INTERVAL);
  time_t wakeUp = 0;
  // Use retry time if earlier than next normal time if retries left
  if ( state.retry && retryWakeup + SYSTEM_TIME_OUT < regularWakeup) {
    wakeUp = retryWakeup;
  } else {
    wakeUp = regularWakeup;
    state.retries = 3;
  }
  // We still have to calculate from now, this could be potentially negative
  // and will be corrected below
  int32_t difference = wakeUp - now;
  // Use MINIMUM sleep time if meassages trigger feedback: config change
  // confirmation or error message
  if (state.config_change_requested) { difference = MINIMUM_SLEEP; }
  // Sleep time takes precidence
  if (state.sleep != 0) { difference = state.sleep; }
  // set minimum sleep time, to ensure we wake up
  difference = ( difference < MINIMUM_SLEEP ) ? MINIMUM_SLEEP : difference;
  // Sleep time is 3 days maximum
  difference = ( difference > MAXIMUM_SLEEP ) ? MAXIMUM_SLEEP : difference;
  // store expected wakeup to account for clock drift later (not implemented)
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
mainFSM helpers::processRockblockMessage(
  systemState &state, char *bfr, bool success=false, bool busy=true
) {
  if (!busy) {
    // RB send success
    if (success) {
      state.mode = NORMAL;
      // Apply new interval AFTER config message sent successfully
      if (state.new_interval != 0) {
        state.interval = state.new_interval;
        state.new_interval = 0;
      }
      // Apply new sleep AFTER config message sent successfully
      if (state.new_sleep != 0) {
        state.sleep = state.new_sleep;
        state.new_sleep = 0;
        state.mode = WAKE_UP;
      }
      // process incoming message, when available
      if (bfr[0] != '\0') {
        if (scoutMessages::parseIncoming(state, bfr)) {
          state.mode = CONFIG;
          state.interval = 600;
        } else {
          state.mode = ERROR;
        }
        // we are using this to determine whether we should send immediately
        // on the first try
        state.config_change_requested = true;
      }
      return SLEEP_READY;
    }
  }
  // keep polling
  return WAIT_FOR_RB;
}

/*
 * Update from GPS (side effect) and return next state
 * :param systemState state: A pointer to the system state
 * :param Gps gps: A pointer to the gps class
 * :param time_t time: Time when function called
 * :param bool timeout: Whether to timeout the GPS
 * :return type mainFSM:
 */
mainFSM helpers::processGpsFix(
  systemState &state, Gps &gps, time_t time, bool timeout=false
) {
    if (!gps.updated && !timeout) { return WAIT_FOR_GPS; }
    if (gps.updated) {
      state.lat = gps.lat;
      state.lng = gps.lng;
      state.heading = gps.heading;
      state.speed = gps.speed;
      state.gps_read_time = gps.get_corrected_epoch();
      if (state.start_time == 0) {
        state.start_time = gps.get_corrected_epoch() - time;
      }
    }
    if (timeout) {
      state.lat = 999;
      state.lng = 999;
      state.heading = 0;
      state.speed = 0;
      state.gps_read_time = time;
    }
    state.gps_done = true;
    return WAIT_FOR_RB;
}
