#include <scoutMessages.h>

/*
 * Take a float value and return NMEA string
 */
void ScoutMessages::float2Nmea(char* bfr, float value, bool latFlag) {
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
    sprintf(bfr+ptr, "%.0f%.4f", whole, mins);
}

/*
 * Create a PK001 message compatible with the MicroPython Firmware by
 * Matt Acidy
 *
 * Example: PK001;lat:3658.56558,NS:N,lon:12200.87904,EW:W,utc:195257.00,sog:2.371,cog:0,sta:00,batt:3.44
 */
void ScoutMessages::createPK001(SystemState &state, char* bfr) {
    char helpBfr[255] = {0};
    memcpy(bfr, (char*) "PK001;lat:", 10);
    float2Nmea(helpBfr, state.getLatitude(), true);

    Serial.print("NMEA: "), Serial.println(helpBfr);
    // Serial.print("lat: "); Serial.print(state.getLatitude());
    // Serial.print(", lng: "); Serial.println(state.getLongitude());
}