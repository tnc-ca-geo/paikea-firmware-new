/*
 * Small helpers removed from main for side effect free testing
 */

#ifndef __HELPERS_H__
#define __HELPERS_H__

#include <Arduino.h>
#include <stateType.h>
#include <scoutMessages.h>
#include <gps.h>

#ifndef MINIMUM_SLEEP
#define MINIMUM_SLEEP 5
#endif
#ifndef MAXIMUM_SLEEP
#define MAXIMUM_SLEEP 259200
#endif
#ifndef RETRY_INTERVAL
#define RETRY_INTERVAL 600
#endif
#ifndef SYSTEM_TIME_OUT
  #define SYSTEM_TIME_OUT 540
#endif

namespace helpers {
  // Calculate wake up time, pegging it to actual time starting at 00:00:00.
  time_t getNextWakeupTime(time_t now, unsigned int delay);
  // Print epoch as time.
  void printTime(const time_t time);
  // Get sleep time and retries from state
  uint32_t getSleepDifference(systemState &state, const time_t now);
  // Update state from incoming message
  mainFSM processRockblockMessage(
    systemState &state, char *bfr, bool success, bool busy);
  // Update state from GPS
  mainFSM processGpsFix(
    systemState &state, Gps &gps, time_t time, bool timeout);
}

#endif