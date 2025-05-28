#include <unity.h>
#include "test_rockblock.h"
#include "test_helpers.h"
#include "test_scoutMessages.h"
#define UNITY_DOUBLE_PRECISION 1e-12

void setUp() {}

void tearDown() {}

int runUnityTests() {
    UNITY_BEGIN();
    // test rockblock
    RUN_TEST(testStrSepMulti);
    RUN_TEST(testfloat2NmeaNumber);
    RUN_TEST(testGetSbdixWithLocation);
    RUN_TEST(testExtractFrame);
    RUN_TEST(testParseFrame);
    RUN_TEST(testParseFrameWeirdFrame);
    RUN_TEST(testParseEmptyFrame);
    RUN_TEST(testFrameParserUncleanStart);
    RUN_TEST(testParseValues);
    RUN_TEST(testPayloadParsing);
    RUN_TEST(testPayloadParsingMultipleLines);
    RUN_TEST(testPayloadParsingMultipleEmpty);
    // test helpers
    RUN_TEST(testGetNextWakeupTime);
    RUN_TEST(testGetSleepDifference);
    RUN_TEST(testUpdateStatefromRbMessage);
    // test Scout messages
    RUN_TEST(test_float2Nmea);
    RUN_TEST(test_epoch2utc);
    RUN_TEST(test_createPK001);
    RUN_TEST(test_createPK001_extended);
    // RUN_TEST(test_createPK101);
    RUN_TEST(test_createPK101_change);
    RUN_TEST(test_parsePK006);
    RUN_TEST(test_parseIncoming_incomplete);
    RUN_TEST(test_parseIncoming_invalid);
    return UNITY_END();
}

void setup() {
    runUnityTests();
}

void loop() {}