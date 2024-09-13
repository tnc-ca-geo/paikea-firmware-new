#include <scoutMessages.h>

/*
 * Take a float value and return NMEA string
 */
size_t ScoutMessages::float2Nmea(char* bfr, float value, bool latFlag) {
    size_t ptr;
    float whole, mins;
    bfr[0] = 0;
    if (latFlag) {
        memcpy(bfr, (char*) "lat:", 4);
    } else {
        memcpy(bfr, (char*) "lon:", 4);
    }
    ptr = strlen(bfr);
    mins = std::modf(value, &whole) * 60;
    ptr = sprintf(bfr+ptr, "%.0f%.4f,", whole, mins);
    if ( latFlag ) {
        if (value >= 0) {
            memcpy(bfr+ptr+1+4, (char*) "NS:N", 5);
        } else {
            memcpy(bfr+ptr+1+4, (char*) "NS:S", 5);
        }
    } else {
if (value >= 0) {
            memcpy(bfr+ptr+1+4, (char*) "EW:E", 5);
        } else {
            memcpy(bfr+ptr+1+4, (char*) "EW:W", 5);
        }
    }
    return ptr + 5 + 5;
}

/*
 * Create a PK001 message compatible with the MicroPython Firmware by
 * Matt Acidy
 *
 * Example: PK001;lat:3658.56558,NS:N,lon:12200.87904,EW:W,utc:195257.00,sog:2.371,cog:0,sta:00,batt:3.44
 */
void ScoutMessages::createPK001(SystemState &state, char* bfr) {
    char helpBfr[255] = {0};
    // sprintf(bfr, 255, "PK001;")
    memcpy(bfr, (char*) "PK001;", 6);
    size_t len = float2Nmea(helpBfr, state.getLatitude(), true);
    memcpy(bfr+6, helpBfr, len);
    Serial.print("Message: "), Serial.println(bfr);
    // Serial.print("lat: "); Serial.print(state.getLatitude());
    // Serial.print(", lng: "); Serial.println(state.getLongitude());
}