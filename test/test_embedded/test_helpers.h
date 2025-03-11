/*
 * Test helpers in main.cpp
 */
#include "stateType.h"
#include "helpers.h"


void testGetSleepDifference() {
    // start_time, frequency, retries, send_success, message_sent, rockblock_done, bat, gps, updated, timeout
    systemState test_data;
    test_data.start_time = 0;
    test_data.frequency = 60;
    test_data.retries = 3;
    test_data.send_success = true;
    uint32_t sleepDifference = getSleepDifference(test_data, 0, 0);
};
