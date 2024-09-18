/*
 * Creating messages for the Scout system. We are using the first letter to
 * distinguish between firmware and message versions. Currently P denotes the
 * old message format that was created by Matt Arcidy and that will work
 * interchangibly with the MicroPython firmware. In the future we will create
 * a new, struct based format with more system feedback.
 */

#ifndef __SCOUT_MESSAGES_H__
#define __SCOUT_MESSAGES_H__

#include <stateType.h>

const std::string EWE = "EW:E";
const std::string EWW = "EW:W";
const std::string NSN = "NS:N";
const std::string NSS = "NS:S";
const std::string LAT = "lat";
const std::string LON = "lon";


class ScoutMessages {

    public:
        static size_t createPK001(systemState &state, char* bfr);
        static size_t float2Nmea(char* bfr, float value, bool latFlag=true);

};

#endif
