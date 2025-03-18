#include <unity.h>
#include "scoutMessages.h"
#include "stateType.h"


ScoutMessages messages;


void test_float2Nmea() {
    char bfr[255] = {0};
    systemState state;
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
    systemState state;
    size_t len;
    len = messages.epoch2utc(bfr, 1726686649);
    TEST_ASSERT_EQUAL_STRING("utc:191049.00", bfr);
}


void test_createPK001() {
    char bfr[255] = {0};
    systemState state;
    state.lat = 35.5;
    state.lng = -122;
    state.gps_read_time = 1726686649;
    state.bat = 4.2;
    messages.createPK001(bfr, state);
    TEST_ASSERT_EQUAL_STRING(
        "PK001;lat:3530.0000,NS:N,lon:12200.0000,EW:W,utc:191049.00,"
        "sog:0.000,cog:0,sta:00,batt:4.20", bfr);
}


void test_parsePK006() {
    char bfr[32] = {0};
    systemState state;
    // Correct message format
    strcpy(bfr, "+DATA:PK006,60");
    TEST_ASSERT(messages.parsePK006(state, bfr));
    TEST_ASSERT_EQUAL_UINT32(60, state.interval);
    // Incmomplete message
    strcpy(bfr, "+DATA");
    TEST_ASSERT_FALSE(messages.parsePK006(state, bfr));
    // Invalid with leading spaces
    state.interval = 44;
    strcpy(bfr, "   +DATA:PK006,33");
    TEST_ASSERT_FALSE(messages.parsePK006(state, bfr));
    // Don't override value if new input is not valid.
    TEST_ASSERT_EQUAL_UINT32(44, state.interval);
    // Non-parsable
    state.interval = 44;
    strcpy(bfr, "+DATA:PK006,Nonsense");
    TEST_ASSERT_FALSE(messages.parsePK006(state, bfr));
    TEST_ASSERT_EQUAL_UINT32(44, state.interval);
    // test different length
    strcpy(bfr, "+DATA:PK006,7");
    TEST_ASSERT_TRUE(messages.parsePK006(state, bfr));
    TEST_ASSERT_EQUAL_UINT32(7, state.interval);
    strcpy(bfr, "+DATA:PK006,1300");
    TEST_ASSERT_TRUE(messages.parsePK006(state, bfr));
    TEST_ASSERT_EQUAL_UINT32(1300, state.interval);
    // test fences
    strcpy(bfr, "+DATA:PK006,4");
    TEST_ASSERT_TRUE(messages.parsePK006(state, bfr));
    TEST_ASSERT_EQUAL_UINT32(5, state.interval);
    strcpy(bfr, "+DATA:PK006,40000");
    TEST_ASSERT_TRUE(messages.parsePK006(state, bfr));
    TEST_ASSERT_EQUAL_UINT32(1440, state.interval);
    // test negative
    state.interval = 44;
    strcpy(bfr, "+DATA:PK006,-4");
    TEST_ASSERT_FALSE(messages.parsePK006(state, bfr));
    TEST_ASSERT_EQUAL_UINT32(44, state.interval);
}