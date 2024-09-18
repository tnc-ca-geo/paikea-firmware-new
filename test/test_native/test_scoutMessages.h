#include <unity.h>
#include "scoutMessages.h"
#include "stateType.h"


systemState state;
ScoutMessages messages;


void test_float2Nmea() {
    char bfr[255] = {0};
    messages.float2Nmea(bfr, 35.5, true);
    TEST_ASSERT_EQUAL_STRING("lat:3530.0000,NS:N", bfr);
    messages.float2Nmea(bfr, -35.5, true);
    TEST_ASSERT_EQUAL_STRING("lat:3530.0000,NS:S", bfr);
    messages.float2Nmea(bfr, -122.5, false);
    TEST_ASSERT_EQUAL_STRING("lon:12230.0000,EW:W", bfr);
    messages.float2Nmea(bfr, -122, false);
    TEST_ASSERT_EQUAL_STRING("lon:12200.0000,EW:W", bfr);
    messages.float2Nmea(bfr, -8.0002, true);
    TEST_ASSERT_EQUAL_STRING("lat:800.0120,NS:S", bfr);
}


void test_epoch2utc() {
    char bfr[255] = {0};
    size_t len;
    len = messages.epoch2utc(bfr, 1726686649);
    TEST_ASSERT_EQUAL_STRING("utc:191049.00", bfr);
}


void test_createPK001() {
    char bfr[255] = {0};
    state.lat = 35.5;
    state.lng = -122;
    state.time_read_system_time = 1726686649;
    state.bat = 4.2;
    messages.createPK001(bfr, state);
    TEST_ASSERT_EQUAL_STRING(
        "PK001;lat:3530.0000,NS:N,lon:12200.0000,EW:W,utc:191049.00,"
        "sog:0,cog:0,sta:00,batt:4.20", bfr);
}