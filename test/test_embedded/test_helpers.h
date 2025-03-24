/*
 * Test helpers in main.cpp
 */
#include "stateType.h"
#include "helpers.h"

using namespace helpers;

void testGetNextWakeupTime() {
    // test 10 min interval
    TEST_ASSERT_EQUAL_INT(1E9 + 200, getNextWakeupTime(1E9, 600)); // Sunday, September 9, 2001 1:50:00 AM
    // test 3 hour interval
    TEST_ASSERT_EQUAL_INT(1E9 + 4400, getNextWakeupTime(1E9, 10800)); // Sunday, September 9, 2001 3:00:00 AM
};


void testGetSleepDifference() {
    // start_time, interval, retries, send_success, message_sent, rockblock_done, bat, gps, updated, timeout
    systemState test_state;
    test_state.start_time = 1E9; // September 9, 2001 1:46:40 AM
    test_state.interval = 600;
    test_state.gps_read_time = 1E9 + 20; // September 9, 2001 1:47:00 AM
    test_state.retries = 3;
    test_state.send_success = true;
    test_state.mode = NORMAL;
    // scenario 1
    // start time: 1:46:40
    // gps read time: 1:47:00
    // interval: 600
    // next wakeup time: 1:50:00
    TEST_ASSERT_EQUAL_INT(160, getSleepDifference(test_state, 1E9 + 40));
    // scenario 2: No GPS read time
    // start time: 1:46:40
    // gps read time: 0:00:00
    // interval: 600
    // next wakeup time: 1:50:00
    test_state.gps_read_time = 0;
    TEST_ASSERT_EQUAL_INT(160, getSleepDifference(test_state, 1E9 + 40));
    // scenario 3: Wakeup time passes while running
    // start time: 1:49:20
    // gps read time: 1:49:40
    // interval: 600
    // next wakeup time: 1:50:00
    test_state.start_time = 1E9 + 140; // September 9, 2001 1:49:20 AM
    test_state.gps_read_time = 1E9 + 180; // September 9, 2001 1:49:40 AM
    TEST_ASSERT_EQUAL_INT(5, getSleepDifference(test_state, 1E9 + 220));
    // scenario 4: Interval time is very short, which will probably end in a
    // negative difference, in which case minimum sleep time is 5 seconds
    // start time: 1:49:20
    // gps read time: 1:49:40
    // interval: 7
    // next wakeup time: 1:49:25
    test_state.interval = 7;
    TEST_ASSERT_EQUAL_INT(5, getSleepDifference(test_state, 1E9 + 220));
    // scenario 5: long interval
    // start time: 1:46:20
    // gps read time: 1:47:00
    // interval: 1800
    // next wakeup time: 2:00:00
    // retry time: 1:50:00
    test_state.start_time = 1E9; // September 9, 2001 1:46:40 AM
    test_state.gps_read_time = 1E9 + 20; // September 9, 2001 1:47:00 AM
    test_state.interval = 1800;
    test_state.retries = 3;  // retries left
    test_state.send_success = true;
    TEST_ASSERT_EQUAL_INT(720, getSleepDifference(test_state, 1E9 + 80));
    TEST_ASSERT_EQUAL_INT(test_state.retries, 3);
    // scenario 6: long interval, no success
    // start time: 1:46:40
    // gps read time: 1:47:00
    // interval: 1800
    // next wakeup time: 2:00:00
    // retry time: 1:50:00
    test_state.mode = RETRY;
    TEST_ASSERT_EQUAL_INT(120, getSleepDifference(test_state, 1E9 + 80));
};


void testUpdateStatefromRbMessage() {
    char bfr[255] = {0};
    systemState test_state;
    test_state.mode = FIRST;
    test_state.gps_done = true;

    // scenario 1, waiting for send success
    TEST_ASSERT_EQUAL_INT((int) WAIT_FOR_RB,
        update_state_from_rb_msg(test_state, bfr, 0, false, false));

    // scenario 2, still busy
    TEST_ASSERT_EQUAL_INT((int) WAIT_FOR_RB,
        update_state_from_rb_msg(test_state, bfr, 0, false, true));

        // scenario 3, simple success without message
    TEST_ASSERT_EQUAL_INT((int) SLEEP_READY,
        update_state_from_rb_msg(test_state, bfr, 0, true, false));
    TEST_ASSERT_EQUAL_INT((int) NORMAL, test_state.mode);
    TEST_ASSERT_EQUAL_INT(600, test_state.interval);
    TEST_ASSERT_EQUAL_INT(0, test_state.new_interval);
    TEST_ASSERT_EQUAL_INT(3, test_state.retries);

    // scenario 4, simple success without message, second run, success should
    // reset retries
    test_state.retries = 1;
    TEST_ASSERT_EQUAL_INT((int) SLEEP_READY,
        update_state_from_rb_msg(test_state, bfr, 0, true, false));
    TEST_ASSERT_EQUAL_INT((int) NORMAL, test_state.mode);
    TEST_ASSERT_EQUAL_INT(600, test_state.interval);
    TEST_ASSERT_EQUAL_INT(0, test_state.new_interval);
    TEST_ASSERT_EQUAL_INT(3, test_state.retries);

    // scenario 5, success with interval messsage
    test_state.retries = 1;
    test_state.interval = 3600;
    strncpy(bfr, "+DATA:PK006,30;\r\n", 32);
    TEST_ASSERT_EQUAL_INT((int) SLEEP_READY,
        update_state_from_rb_msg(test_state, bfr, 0, true, false));
    TEST_ASSERT_EQUAL_INT((int) CONFIG, test_state.mode);
    TEST_ASSERT_EQUAL_INT(600, test_state.interval);
    TEST_ASSERT_EQUAL_INT(1800, test_state.new_interval);
    TEST_ASSERT_EQUAL_INT(3, test_state.retries);
}