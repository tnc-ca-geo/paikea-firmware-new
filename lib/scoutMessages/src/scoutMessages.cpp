#include <scoutMessages.h>
// required for snprintf
#include <inttypes.h>

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
 * Create utc time hhmmss as it used in PK101 messages.
 */
size_t scoutMessages::epoch2utcSimple(char* bfr, time_t val) {
    struct tm *tmp = gmtime(&val);
    return snprintf(
        bfr, 16, "utc:%02d%02d%02d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
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
 * Example: PK001;lat:3658.56558,NS:N,lon:12200.87904,EW:W,utc:195257.00,sog:2.371,cog:0,sta:00,batt:3.44,int:10,st:5
 */
size_t scoutMessages::createPK001_extended(char* bfr, const systemState state) {
    size_t len = createPK001(bfr, state);
    uint16_t interval = (
        state.new_interval == 0) ? state.interval : state.new_interval;
    return snprintf(bfr+len, 128-len, ",int:%d,st:%d", (int) interval/60,
        (int) state.mode);
}

/*
 * Create modified PK101 message
 * - sog, removed
 * - cog, removed
 * - sta, removed
 * - sl, added, sleep time in minutes
 * - int, added, interval in minutes
 * - st, added, status 0, 1, 2, 3, 4
 */
size_t scoutMessages::createPK101(char* bfr, const systemState state) {
    // wasting some memory here
    char latBfr[32] = {0};
    char lonBfr[32] = {0};
    char timeBfr[16] = {0};
    float2Nmea(latBfr, state.lat, true);
    float2Nmea(lonBfr, state.lng, false);
    epoch2utcSimple(timeBfr, state.gps_read_time);
    uint32_t interval = (
        state.new_interval == 0) ? state.interval : state.new_interval;
    uint32_t sleep = (
        state.new_sleep == 0) ? state.sleep : state.new_sleep;
    return snprintf(
        bfr, 128, "PK101;%s,%s,%s,batt:%.1f,int:%d,sl:%d,st:%d",
        latBfr, lonBfr, timeBfr, state.bat, (uint32_t) interval/60,
        (uint32_t) sleep/60, state.mode);
}

/*
 * Parse an incoming message. The data format is rather inconsistent,
 * but we are taking it from the legacy version of the firmware by Matt Arcady.
 * Example: +DATA:PK006,60 (minutes) BUT +DATA:PK007:86400 (seconds)
 */
bool scoutMessages::parseIncoming(systemState &state, char* bfr) {
    // with more tokens we should use enum but keep it simple for now
    uint8_t message_type = 0; // 1 PK001, 2 PK002
    int32_t parsed = 0;
    const char* tokens[] = {"+DATA:PK006,", "+DATA:PK007,"};
    // initialize values
    state.new_interval = 0;
    state.new_sleep = 0;
    // check tokens
    for (int i=0; i<2; i++) {
      char* substr = strstr(bfr, tokens[i]);
      if (substr == NULL) { continue; }
      if (substr != bfr) { continue; }
      message_type = i + 1;
      break;
    }
    if (message_type == 0) { return false; }
    // this works only because all tokens have the exact same length so far
    try { parsed = std::stoi(bfr+12, nullptr, 10); }
    catch (...) { return false; };
    // values below zero are not allowed for both message types
    if (parsed < 0) { return false; }
    // this is in minutes
    if (message_type == 1 && parsed > 1440 ) { return false; }
    // this is in seconds
    if (message_type == 2 && parsed > 259200 ) { return false; }
    // finally set times
    if (message_type == 1) { state.new_interval = parsed * 60; }
    if (message_type == 2) {
        state.new_interval = 600;
        state.new_sleep = parsed;
    }
    return true;
}