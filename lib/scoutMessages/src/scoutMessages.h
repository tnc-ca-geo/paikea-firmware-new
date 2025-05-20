/*
 * Creating messages for the Scout system. We are using the first letter to
 * distinguish between firmware and message versions. Currently P denotes the
 * old message format that was created by Matt Arcidy and that will work
 * interchangibly with the MicroPython firmware. In the future we will create
 * a new, struct based format with more system feedback.
 *
 * The parsing class is a translater between system state and messages.
 */

#ifndef __SCOUT_MESSAGES_H__
#define __SCOUT_MESSAGES_H__

#include <Arduino.h>
#include <time.h>
#include <stateType.h>

namespace scoutMessages {
  size_t epoch2utc(char* bfr, time_t val);
  // create a simpler version since we don't need to transmit .00 with
  // every message
  size_t epoch2utcSimple(char* bfr, time_t val);
  size_t float2Nmea(char* bfr, float val, bool latFlag=true);
  size_t createPK001(char* bfr, const systemState state);
  // TODO: reimplement original PK001 if needed
  // PK001_extended adds extra fields to PK001, for development only 
  size_t createPK001_extended(char* bfr, const systemState state);
  // Modified version of PK101 as used in v3
  size_t createPK101(char* bfr, const systemState state);
  bool parseIncoming(systemState &state, char* bfr);
};

#endif
