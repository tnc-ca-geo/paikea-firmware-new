#include <unity.h>
#include "scoutMessages.h"
#include "stateType.h"

using namespace scoutMessages;

void test_float2Nmea() {
  char bfr[255] = {0};
  systemState state;
  float2Nmea(bfr, 35.5, true);
  TEST_ASSERT_EQUAL_STRING("lat:3530.0000,NS:N", bfr);
  float2Nmea(bfr, -35.5, true);
  TEST_ASSERT_EQUAL_STRING("lat:3530.0000,NS:S", bfr);
  float2Nmea(bfr, -122.5, false);
  TEST_ASSERT_EQUAL_STRING("lon:12230.0000,EW:W", bfr);
  float2Nmea(bfr, -122, false);
  TEST_ASSERT_EQUAL_STRING("lon:12200.0000,EW:W", bfr);
  float2Nmea(bfr, -8.0002, true);
  TEST_ASSERT_EQUAL_STRING("lat:800.0120,NS:S", bfr);
}

void test_epoch2utc() {
  char bfr[255] = {0};
  systemState state;
  epoch2utc(bfr, 1726686649);
  TEST_ASSERT_EQUAL_STRING("utc:191049.00", bfr);
}

void test_createPK001() {
  char bfr[255] = {0};
  systemState state;
  state.lat = 35.5;
  state.lng = -122;
  state.gps_read_time = 1726686649;
  state.bat = 4.2;
  createPK001(bfr, state);
  TEST_ASSERT_EQUAL_STRING(
    "PK001;lat:3530.0000,NS:N,lon:12200.0000,EW:W,utc:191049.00,"
    "sog:0.000,cog:0,sta:00,batt:4.20", bfr);
}

void test_createPK001_extended() {
  char bfr[255] = {0};
  systemState state;
  state.lat = 35.5;
  state.lng = -122;
  state.gps_read_time = 1726686649;
  state.bat = 4.2;
  createPK001_extended(bfr, state);
  TEST_ASSERT_EQUAL_STRING(
    "PK001;lat:3530.0000,NS:N,lon:12200.0000,EW:W,utc:191049.00,"
    "sog:0.000,cog:0,sta:00,batt:4.20,int:10,st:0", bfr);
}

void test_createPK101() {
  char bfr[255] = {0};
  systemState state;
  state.lat = 35.5;
  state.lng = -122;
  state.gps_read_time = 1726686649;
  state.bat = 4.2;
  state.interval = 600; // internally we are using seconds but minutes in messages
  state.new_interval = 0;
  state.sleep = 0;
  state.mode = RETRY;
  createPK101(bfr, state);
  TEST_ASSERT_EQUAL_STRING(
    "PK101;lat:3530.0000,NS:N,lon:12200.0000,EW:W,utc:191049,"
    "batt:4.2,int:10,sl:0,st:2", bfr);
}

void test_createPK101_change() {
  char bfr[255] = {0};
  systemState state;
  state.lat = 35.5;
  state.lng = -122;
  state.gps_read_time = 1726686649;
  state.bat = 4.2;
  state.interval = 600; // internally we are using seconds but minutes in messages
  state.new_interval = 1200;
  state.sleep = 0;
  state.mode = CONFIG;
  createPK101(bfr, state);
  TEST_ASSERT_EQUAL_STRING(
    "PK101;lat:3530.0000,NS:N,lon:12200.0000,EW:W,utc:191049,"
    "batt:4.2,int:20,sl:0,st:3", bfr);
}

void test_parsePK006() {
  char bfr[32] = {0};
  systemState state;
  // Correct message format
  strcpy(bfr, "+DATA:PK006,60;");
  TEST_ASSERT_TRUE(parseIncoming(state, bfr));
  TEST_ASSERT_EQUAL_UINT32(3600, state.new_interval);
  // short numbers
  strcpy(bfr, "+DATA:PK006,6;");
  TEST_ASSERT_TRUE(parseIncoming(state, bfr));
  TEST_ASSERT_EQUAL_UINT32(360, state.new_interval);
  // long numbers
  strcpy(bfr, "+DATA:PK006,600;");
  TEST_ASSERT_TRUE(parseIncoming(state, bfr));
  TEST_ASSERT_EQUAL_UINT32(36000, state.new_interval);
}

// incomplete message
void test_parseIncoming_incomplete() {
  char bfr[32] = {};
  systemState state;
  strcpy(bfr, "+DATA");
  TEST_ASSERT_FALSE(parseIncoming(state, bfr));
  TEST_ASSERT_EQUAL_INT(0, state.new_interval);
}

void test_parseIncoming_invalid() {
  // invalid with leading spaces
  {
    char bfr[32] = {};
    systemState state;
    strcpy(bfr, "   +DATA:PK006,33");
    TEST_ASSERT_FALSE(parseIncoming(state, bfr));
    TEST_ASSERT_EQUAL_INT(0, state.new_interval);
  }
  // nonsense content
  {
    char bfr[32] = {};
    systemState state;
    strcpy(bfr, "+DATA:PK006,nonsense;");
    TEST_ASSERT_FALSE(parseIncoming(state, bfr));
    TEST_ASSERT_EQUAL_INT(0, state.new_interval);
  }
  // negative values, lower fence
  {
    char bfr[32] = {};
    systemState state;
    strcpy(bfr, "+DATA:PK006,-10;");
    TEST_ASSERT_FALSE(parseIncoming(state, bfr));
    TEST_ASSERT_EQUAL_INT(0, state.new_interval);
  }
  // test upper fence
  {
    char bfr[32] = {};
    systemState state;
    strcpy(bfr, "+DATA:PK006,1441;");
    TEST_ASSERT_FALSE(parseIncoming(state, bfr));
    TEST_ASSERT_EQUAL_INT(0, state.new_interval);
  }
}

void test_parsePK007() {
    char bfr[32] = {0};
  systemState state;
  // Correct message format
  strcpy(bfr, "+DATA:PK007,600;");
  TEST_ASSERT_TRUE(parseIncoming(state, bfr));
  TEST_ASSERT_EQUAL_UINT32(600, state.new_sleep);
  TEST_ASSERT_EQUAL_UINT32(600, state.new_interval);
  // short numbers
  strcpy(bfr, "+DATA:PK006,6;");
  TEST_ASSERT_TRUE(parseIncoming(state, bfr));
  TEST_ASSERT_EQUAL_UINT32(6, state.new_sleep);
  TEST_ASSERT_EQUAL_UINT32(600, state.new_interval);
  // long numbers, testing fence
  strcpy(bfr, "+DATA:PK006,259201;");
  state.interval = 1200;
  TEST_ASSERT_FALSE(parseIncoming(state, bfr));
  TEST_ASSERT_EQUAL_UINT32(0, state.new_sleep);
  TEST_ASSERT_EQUAL_UINT32(1200, state.new_interval);
}
