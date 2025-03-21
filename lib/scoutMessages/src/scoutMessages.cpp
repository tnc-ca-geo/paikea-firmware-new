#include <scoutMessages.h>

#ifndef DEFAULT_INTERVAL
#define DEFAULT INTERVAL 600
#endif

/*
 * Take a float value and return NMEA string for lat and long.
 */
size_t scoutMessages::float2Nmea(char* bfr, float val, bool latFlag) {
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
size_t scoutMessages::epoch2utc(char* bfr, time_t val) {
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
size_t scoutMessages::createPK001(char* bfr, const systemState state) {
    // wasting some memory here
    char latBfr[32] = {0};
    char lonBfr[32] = {0};
    char timeBfr[16] = {0};
    float2Nmea(latBfr, state.lat, true);
    float2Nmea(lonBfr, state.lng, false);
    epoch2utc(timeBfr, state.gps_read_time);
    return snprintf(
        bfr, 128, "PK001;%s,%s,%s,sog:%.3f,cog:%.0f,sta:00,batt:%.2f",
        latBfr, lonBfr, timeBfr, state.speed, state.heading, state.bat);
}

/*
 * Create an extended PK001 message
 *
 * Example: PK001;lat:3658.56558,NS:N,lon:12200.87904,EW:W,utc:195257.00,sog:2.371,cog:0,sta:00,batt:3.44,int:5
 */
size_t scoutMessages::createPK001_extended(char* bfr, const systemState state) {
    size_t len = createPK001(bfr, state);
    uint16_t interval = (
        state.new_interval == 0) ? state.interval : state.new_interval;
    return snprintf(bfr+len, 128-len, ",int:%d,st:%d", (int) interval/60,
        (int) state.mode);
}

/*
 * Parse an incoming message. The data format is rather inconsistent,
 * but we are taking it from the legacy version of the firmware by Matt Arcady.
 * Example: +DATA:PK006,60
 */
bool scoutMessages::parseIncoming(systemState &state, char* bfr) {
    char token[] = "+DATA:PK006,";
    char* substr = strstr(bfr, token);
    int32_t parsed = 0;
    if (substr == NULL) return false;
    if (substr != bfr) return false;
    try { parsed = std::stoi(bfr+12, nullptr, 10); }
    catch (...) { return false; }
    if (parsed < 0) { return false; }
    else if (parsed < 2) { state.new_interval = 2 * 60; }
    else if (parsed > 1440 ) { state.new_interval = 1440 * 60; }
    else { state.new_interval = parsed  * 60; }
    return true;
}