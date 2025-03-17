#include <unity.h>
#include "test_rockblock.h"
#include "test_helpers.h"
#define UNITY_DOUBLE_PRECISION 1e-12

void setUp() {}

void tearDown() {}

int runUnityTests() {
    UNITY_BEGIN();
    // test rockblock
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
    return UNITY_END();
}

void setup() {
    runUnityTests();
}

void loop() {}