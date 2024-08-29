#include <unity.h>
#include "test_state.h"
#include "test_loraRockblock.h"
#define UNITY_INCLUDE_DOUBLE
#define UNITY_DOUBLE_PRECISION 1e-12


void setUp() {}

void tearDown() {}

int runUnityTests() {
    UNITY_BEGIN();
    // test the state object
    RUN_TEST(test_state_init);
    RUN_TEST(test_state_realtime);
    RUN_TEST(test_next_send_time);
    // testy loraRockblock
    RUN_TEST(test_parse_integer);
    return UNITY_END();
}

void setup() {
    runUnityTests();
}

void loop() {}