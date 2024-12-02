#include <unity.h>
#include "test_loraRockblock.h"
#ifndef UNITY_INCLUDE_DOUBLE
#define UNITY_INCLUDE_DOUBLE
#endif
#define UNITY_DOUBLE_PRECISION 1e-12


void setUp() {}

void tearDown() {}

int runUnityTests() {
    UNITY_BEGIN();
    // testy loraRockblock
    RUN_TEST(test_parse_integer);
    return UNITY_END();
}

void setup() {
    runUnityTests();
}

void loop() {}