/*
 * Test helpers in main.cpp
 */
#include "stateType.h"
#include "helpers.h"
#include <unity.h>

using namespace helpers;

void testGetNextWakeupTime() {
    // test 10 min interval
    TEST_ASSERT_EQUAL_INT(1E9 + 200, getNextWakeupTime(1E9, 600)); // Sunday, September 9, 2001 1:50:00 AM
    // test 3 hour interval
    TEST_ASSERT_EQUAL_INT(1E9 + 4400, getNextWakeupTime(1E9, 10800)); // Sunday, September 9, 2001 3:00:00 AM
};

void testGetSleepDifference() {

    // scenario 1, newly started system
    {
      systemState test_state;
      time_t now = 1E9 + 40; // September 9, 2001 1:47:20 AM
      test_state.expected_wakeup = 0; // not set yet
      test_state.gps_read_time = 1E9 + 20; // September 9, 2001 1:47:00 AM
      test_state.mode = NORMAL;
      uint32_t res = getSleepDifference(test_state, now); // 1:50:00 AM
      TEST_ASSERT_EQUAL_INT(1E9+200, test_state.expected_wakeup);
      TEST_ASSERT_EQUAL_INT(160, res);
    }

    // scenario 2: newly started system no GPS read time
    {
      systemState test_state;
      time_t now = 230; // January 1, 1970 12:03:50 AM
      test_state.expected_wakeup = 0; // not yet set
      test_state.gps_read_time = 0; // not available
      test_state.mode = NORMAL;
      TEST_ASSERT_EQUAL_INT(370, getSleepDifference(test_state, now));
    }

    // scenario 3: wakeup time passes while running, newly started system
    {
      systemState test_state;
      time_t now = 1E9 + 300; // 01:51:40
      test_state.gps_read_time = 1E9 + 20; // 01:47:00
      test_state.mode = NORMAL;
      TEST_ASSERT_EQUAL_INT(5, getSleepDifference(test_state, now));
      TEST_ASSERT_EQUAL_INT(1E9 + 305, test_state.expected_wakeup);
    }

    // scenario 4: Interval time is very short
    {
      systemState test_state;
      time_t now = 1E9 + 220;
      test_state.interval = 7;
      test_state.mode = NORMAL;
      TEST_ASSERT_EQUAL_INT(5, getSleepDifference(test_state, now));
    }

    // scenario 5: long interval
    {
      systemState test_state;
      time_t now = 1E9 + 80;
      test_state.gps_read_time = 1E9 + 20; // September 9, 2001 1:47:00 AM
      test_state.interval = 1800;
      TEST_ASSERT_EQUAL_INT(720, getSleepDifference(test_state, now));
      TEST_ASSERT_EQUAL_INT(test_state.retries, 3);
      TEST_ASSERT_EQUAL_INT(1E9 + 800, test_state.expected_wakeup);
    } 

    // scenario 6: long interval, no success
    {
      systemState test_state;
      test_state.gps_read_time = 1E9 + 20; // September 9, 2001 1:47:00 AM
      test_state.interval = 1800;
      test_state.retry = true;
      // test_state.mode = WAKE_UP;
      TEST_ASSERT_EQUAL_INT(120, getSleepDifference(test_state, 1E9 + 80));
      TEST_ASSERT_EQUAL_INT(1E9 + 200, test_state.expected_wakeup);
    }
};


void testUpdateStatefromRbMessage() {
    char bfr[255] = {0};
    systemState test_state;
    test_state.mode = FIRST;
    test_state.gps_done = true;

    // scenario 1, waiting for send success
    TEST_ASSERT_EQUAL_INT((int) WAIT_FOR_RB,
        processRockblockMessage(test_state, bfr, false, false));

    // scenario 2, still busy
    TEST_ASSERT_EQUAL_INT((int) WAIT_FOR_RB,
        processRockblockMessage(test_state, bfr, false, true));

        // scenario 3, simple success without message
    TEST_ASSERT_EQUAL_INT((int) SLEEP_READY,
        processRockblockMessage(test_state, bfr, true, false));
    TEST_ASSERT_EQUAL_INT((int) NORMAL, test_state.mode);
    TEST_ASSERT_EQUAL_INT(600, test_state.interval);
    TEST_ASSERT_EQUAL_INT(0, test_state.new_interval);
    TEST_ASSERT_EQUAL_INT(3, test_state.retries);

    // scenario 4, simple success without message, second run, success should
    // reset retries
    TEST_ASSERT_EQUAL_INT((int) SLEEP_READY,
        processRockblockMessage(test_state, bfr, true, false));
    TEST_ASSERT_EQUAL_INT((int) NORMAL, test_state.mode);
    TEST_ASSERT_EQUAL_INT(600, test_state.interval);
    TEST_ASSERT_EQUAL_INT(0, test_state.new_interval);

    // scenario 5, success with interval messsage
    test_state.interval = 3600;
    strncpy(bfr, "+DATA:PK006,30;\r\n", 32);
    TEST_ASSERT_EQUAL_INT((int) SLEEP_READY,
        processRockblockMessage(test_state, bfr, true, false));
    TEST_ASSERT_EQUAL_INT((int) CONFIG, test_state.mode);
    TEST_ASSERT_EQUAL_INT(600, test_state.interval);
    TEST_ASSERT_EQUAL_INT(1800, test_state.new_interval);

    // scenario 6, success with interval and sleep message
    test_state.interval = 60;
    strncpy(bfr, "+DATA:PK007,7200;\r\n", 32);
    TEST_ASSERT_EQUAL_INT((int) SLEEP_READY,
        processRockblockMessage(test_state, bfr, true, false));
    TEST_ASSERT_EQUAL_INT((int) CONFIG, test_state.mode);
    TEST_ASSERT_EQUAL_INT(600, test_state.interval);
    TEST_ASSERT_EQUAL_INT(7200, test_state.new_sleep);

    // scenario 7, nonsensical message
    test_state.retries = 1;
    test_state.new_interval = 0;
    test_state.new_sleep = 0;
    test_state.mode = NORMAL;
    test_state.gps_done = true;
    test_state.interval = 1200;
    strncpy(bfr, "NONSENSE;\r\n", 32);
    TEST_ASSERT_EQUAL_INT((int) SLEEP_READY,
        processRockblockMessage(test_state, bfr, true, false));
    TEST_ASSERT_EQUAL_INT((int) ERROR, test_state.mode);
    TEST_ASSERT_EQUAL_INT(1200, test_state.interval);
    TEST_ASSERT_EQUAL_INT(0, test_state.new_interval);
    TEST_ASSERT_EQUAL_INT(0, test_state.new_sleep);

    // scenario 8, Message out of bounds
    test_state.retries = 1;
    test_state.new_interval = 0;
    test_state.new_sleep = 0;
    test_state.mode = NORMAL;
    test_state.gps_done = true;
    test_state.interval = 1200;
    strncpy(bfr, "+DATA:PK006,2000;\r\n", 32);
    TEST_ASSERT_EQUAL_INT((int) SLEEP_READY,
        processRockblockMessage(test_state, bfr, true, false));
    TEST_ASSERT_EQUAL_INT((int) ERROR, test_state.mode);
    TEST_ASSERT_EQUAL_INT(1200, test_state.interval);
}