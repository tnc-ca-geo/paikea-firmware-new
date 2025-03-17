/*
 * Test helpers in main.cpp
 */
#include "stateType.h"
#include "helpers.h"


void testGetNextWakeupTime() {
    // test 10 min interval
    TEST_ASSERT_EQUAL_INT(1E9 + 200, getNextWakeupTime(1E9, 600)); // Sunday, September 9, 2001 1:50:00 AM
    // test 3 hour interval
    TEST_ASSERT_EQUAL_INT(1E9 + 4400, getNextWakeupTime(1E9, 10800)); // Sunday, September 9, 2001 3:00:00 AM
};


void testGetSleepDifference() {
    // start_time, frequency, retries, send_success, message_sent, rockblock_done, bat, gps, updated, timeout
    systemState test_state;
    test_state.start_time = 1E9; // September 9, 2001 1:46:40 AM
    test_state.frequency = 600;
    test_state.gps_read_time = 1E9 + 20; // September 9, 2001 1:47:00 AM
    test_state.retries = 3;
    test_state.send_success = true;
    // make sure constants are set correctly
    TEST_ASSERT_EQUAL_INT(600, test_state.retry_time);
    // scenario 1
    // start time: 1:46:40
    // gps read time: 1:47:00
    // frequency: 600
    // next wakeup time: 1:50:00
    TEST_ASSERT_EQUAL_INT(160, getSleepDifference(test_state, 1E9 + 40));
    // scenario 2: No GPS read time
    // start time: 1:46:40
    // gps read time: 0:00:00
    // frequency: 600
    // next wakeup time: 1:50:00
    test_state.gps_read_time = 0;
    TEST_ASSERT_EQUAL_INT(160, getSleepDifference(test_state, 1E9 + 40));
    // scenario 3: Wakeup time passes while running
    // start time: 1:49:20
    // gps read time: 1:49:40
    // frequency: 600
    // next wakeup time: 1:50:00
    test_state.start_time = 1E9 + 140; // September 9, 2001 1:49:20 AM
    test_state.gps_read_time = 1E9 + 180; // September 9, 2001 1:49:40 AM
    TEST_ASSERT_EQUAL_INT(5, getSleepDifference(test_state, 1E9 + 220));
    // scenario 4: Interval time is very short, which will probably end in a
    // negative difference, in which case minimum sleep time is 5 seconds
    // start time: 1:49:20
    // gps read time: 1:49:40
    // frequency: 7
    // next wakeup time: 1:49:25
    test_state.frequency = 7;
    TEST_ASSERT_EQUAL_INT(5, getSleepDifference(test_state, 1E9 + 220));
    // scenario 5: long interval
    // start time: 1:46:20
    // gps read time: 1:47:00
    // frequency: 1800
    // next wakeup time: 2:00:00
    // retry time: 1:50:00
    test_state.start_time = 1E9; // September 9, 2001 1:46:40 AM
    test_state.gps_read_time = 1E9 + 20; // September 9, 2001 1:47:00 AM
    test_state.frequency = 1800;
    test_state.retries = 3;  // retries left
    test_state.send_success = true;
    TEST_ASSERT_EQUAL_INT(720, getSleepDifference(test_state, 1E9 + 80));
    TEST_ASSERT_EQUAL_INT(test_state.retries, 3);
    // scenario 6: long interval, no success
    // start time: 1:46:40
    // gps read time: 1:47:00
    // frequency: 1800
    // next wakeup time: 2:00:00
    // retry time: 1:50:00
    test_state.send_success = false;
    TEST_ASSERT_EQUAL_INT(120, getSleepDifference(test_state, 1E9 + 80));
};
