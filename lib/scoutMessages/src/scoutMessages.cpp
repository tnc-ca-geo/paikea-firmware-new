#include <scoutMessages.h>

/*
 * Take a float value and return NMEA string for lat and long.
 */
size_t ScoutMessages::float2Nmea(char* bfr, float value, bool latFlag) {
    size_t ptr;
    float whole, mins;
    if (latFlag) {
        memcpy(bfr, (char*) "lat:", 4);
    } else {
        memcpy(bfr, (char*) "lon:", 4);
    }
    mins = abs(std::modf(value, &whole)) * 60;
    whole = abs(whole);
    ptr = snprintf(bfr+4, 16, "%.0f%07.4f,", whole, mins);
    if ( latFlag ) {
        if (value >= 0) {
            memcpy(bfr+ptr+4, (char*) "NS:N", 5);
        } else {
            memcpy(bfr+ptr+4, (char*) "NS:S", 5);
        }
    } else {
        if (value >= 0) {
            memcpy(bfr+ptr+4, (char*) "EW:E", 5);
        } else {
            memcpy(bfr+ptr+4, (char*) "EW:W", 5);
        }
    }
    return ptr + 9;
}

/*
 * Create a PK001 message compatible with the MicroPython Firmware by
 * Matt Acidy
 *
 * Example: PK001;lat:3658.56558,NS:N,lon:12200.87904,EW:W,utc:195257.00,sog:2.371,cog:0,sta:00,batt:3.44
 */
size_t ScoutMessages::createPK001(systemState &state, char* bfr) {
    char latBfr[32] = {0};
    char lonBfr[32] = {0};
    size_t len = 0;
    float2Nmea(latBfr, state.lat, true);
    float2Nmea(lonBfr, state.lng, false);
    len = snprintf(bfr, 255, "PK001;%s,%s", latBfr, lonBfr);
    bfr[len+1] = 0;
    return len;
}