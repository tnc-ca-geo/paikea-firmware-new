/*
 * Creating messages for the Scout system.
 */

#ifndef __SCOUT_MESSAGES_H__
#define __SCOUT_MESSAGES_H__

#include <state.h>

class ScoutMessages {

    private:
        static size_t float2Nmea(char* bfr, float value, bool latFlag=true);

    public:
        static void createPK001(SystemState &state, char* bfr);

};

#endif
