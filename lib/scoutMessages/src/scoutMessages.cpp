#include <scoutMessages.h>

/*
 * Take a float value and return NMEA string for lat and long.
 */
size_t ScoutMessages::float2Nmea(char* bfr, float val, bool latFlag) {
    size_t ptr;
    float whole;
    float mins = abs(std::modf(val, &whole)) * 60;
    return snprintf(bfr, 32, "%s:%.0f%07.4f,%s",
        latFlag ? "lat" : "lon", abs(whole), mins,
        latFlag ? (val >= 0 ? "NS:N" : "NS:S") : val >= 0 ? "EW:E" : "EW:W");
}

/*
 * Create utc time hhmmss.00 as it used in PK001 messages.
 */
size_t ScoutMessages::epoch2utc(char* bfr, time_t val) {
    struct tm *tmp = gmtime(&val);
    return snprintf(
        bfr, 16, "utc:%02d%02d%02d.00", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
}

/*
 * Create a PK001 message compatible with the MicroPython Firmware by
 * Matt Acidy
 *
 * Example: PK001;lat:3658.56558,NS:N,lon:12200.87904,EW:W,utc:195257.00,sog:2.371,cog:0,sta:00,batt:3.44
 */
size_t ScoutMessages::createPK001(char* bfr, systemState &state) {
    // wasting some memory here
    char latBfr[32] = {0};
    char lonBfr[32] = {0};
    char timeBfr[16] = {0};
    float2Nmea(latBfr, state.lat, true);
    float2Nmea(lonBfr, state.lng, false);
    epoch2utc(timeBfr, state.gps_read_time);
    return snprintf(
        bfr, 128, "PK001;%s,%s,%s,sog:0,cog:0,sta:00,batt:%.2f",
        latBfr, lonBfr, timeBfr, state.bat);
}

/*
 * Parse an incoming PK006 message. The data format is rather inconsistent,
 * but we are taking it from the legacy version of the firmware by Matt Arcady.
 * Example: +DATA:PK006,60
 */
bool ScoutMessages::parsePK006(systemState &state, char* bfr) {
    char token[] = "+DATA:PK006,";
    char* substr = strstr(bfr, token);
    int32_t parsed = 0;
    if (substr == NULL) return false;
    if (substr != bfr) return false;
    try {
        parsed = std::stoi(bfr+12, nullptr, 10);
    }
    catch (...) { return false; }
    if (parsed < 0) {
        return false;
    } else if (parsed < 5) {
        state.frequency = 5;
    } else if (parsed > 1440 ) {
        state.frequency = 1440;
    } else {
        state.frequency = parsed;
    }
    return true;
}