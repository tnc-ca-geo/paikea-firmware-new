#include <unity.h>
#include "test_rockblock.h"
#define UNITY_INCLUDE_DOUBLE
#define UNITY_DOUBLE_PRECISION 1e-12

void setUp() {}

void tearDown() {}

int runUnityTests() {
    UNITY_BEGIN();
    // test rockblock
    RUN_TEST(test_first);
    return UNITY_END();
}

void setup() {
    runUnityTests();
}

void loop() {}