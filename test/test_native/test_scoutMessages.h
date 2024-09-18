#include <unity.h>
#include "scoutMessages.h"
#include "stateType.h"


systemState state;
ScoutMessages messages = ScoutMessages();


void test_float2Nmea() {
    char bfr[255] = {0};
    size_t len = messages.float2Nmea(bfr, 35.5, true);
    TEST_ASSERT_EQUAL_UINT8(19, len);
    TEST_ASSERT_EQUAL_STRING("lat:3530.0000,NS:N", bfr);
    len = messages.float2Nmea(bfr, -35.5, true);
    TEST_ASSERT_EQUAL_UINT8(19, len);
    TEST_ASSERT_EQUAL_STRING("lat:3530.0000,NS:S", bfr);
    len = messages.float2Nmea(bfr, -122.5, false);
    TEST_ASSERT_EQUAL_UINT8(20, len);
    TEST_ASSERT_EQUAL_STRING("lon:12230.0000,EW:W", bfr);
    len = messages.float2Nmea(bfr, -122, false);
    TEST_ASSERT_EQUAL_UINT8(20, len);
    TEST_ASSERT_EQUAL_STRING("lon:12200.0000,EW:W", bfr);
    len = messages.float2Nmea(bfr, -8.0002, true);
    TEST_ASSERT_EQUAL_UINT8(18, len);
    TEST_ASSERT_EQUAL_STRING("lat:800.0120,NS:S", bfr);
}


void test_createPK001() {
    char bfr[255] = {0};
    size_t len;
    state.lat = 35.5;
    state.lng = -122;
    len = messages.createPK001(state, bfr);
    TEST_ASSERT_EQUAL_UINT8(44, len);
    TEST_ASSERT_EQUAL_STRING(
        "PK001;lat:3530.0000,NS:N,lon:12200.0000,EW:W", bfr);
}