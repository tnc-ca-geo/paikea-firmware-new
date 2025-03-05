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

#include <time.h>
#include <stateType.h>


class ScoutMessages {

    private:
        static size_t epoch2utc(char* bfr, time_t val);
        static size_t float2Nmea(char* bfr, float val, bool latFlag=true);

    public:
        static size_t createPK001(char* bfr, systemState &state);
        bool parseIncoming(systemState &state, char* bfr);

};

#endif
