#include <unity.h>

void setUp() {}

void tearDown() {}

void test_something() {
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_something);
    return UNITY_END();
}

void setup() {
    delay(1000);
    runUnityTests();
}

void loop() {}