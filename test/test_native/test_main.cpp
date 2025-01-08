#include <unity.h>
#include "test_scoutMessages.h"
#include "test_rockBlock.h"

void setUp() {}

void tearDown() {}

/*
 * Be aware of the fact that the structure of a native test project is somewhat
 * different from the embedded tests running on the hardware. Notably, we run in
 * a main function instead of in a loop.
 */
int main() {
    UNITY_BEGIN();
    // test the state object
    RUN_TEST(test_createPK001);
    RUN_TEST(test_float2Nmea);
    RUN_TEST(test_epoch2utc);
    RUN_TEST(test_parsePK006);
    RUN_TEST(test_test);
    return UNITY_END();
}
