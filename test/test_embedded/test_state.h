#include "state.h"
#include <unity.h>


SystemState state = SystemState();


void test_state_init() {
    state.init();
    TEST_ASSERT_TRUE(state.getRealTime() == 0);
    // this has to be global variable, add setter and getter for better testing?
    rtc_expected_wakeup = 315532800;
    state.init();
    // over a second already passed since start
    TEST_ASSERT_TRUE(state.getRealTime() == 315532800);
}

void test_state_realtime() {
    // this has to be global variable, add setter and getter for better testing?
    rtc_expected_wakeup = 0;
    state.init();
    TEST_ASSERT_TRUE(state.getRealTime() == 0);
    delay(1100);
    state.sync();
    TEST_ASSERT_TRUE(state.getRealTime() > 0);
    TEST_ASSERT_TRUE(state.getRealTime() < esp_timer_get_time()/1E6);
    state.setRealTime(79437600, true);
    TEST_ASSERT_TRUE(state.getRealTime() == 79437600);
    // rtc_start should be less than current time
    TEST_ASSERT_LESS_OR_EQUAL_UINT(79437600, rtc_start);
    // less than two seconds passed since system start
    TEST_ASSERT_LESS_OR_EQUAL_UINT(rtc_start, 79437600-2);
    delay(1100);
    TEST_ASSERT_TRUE(state.getRealTime() == 79437600);
    state.sync();
    TEST_ASSERT_TRUE(state.getRealTime() > 79437600);
    TEST_ASSERT_TRUE(state.getRealTime() < 79437602);
}

void test_next_send_time() {
    rtc_frequency = 300;
    state.init();
    state.setRealTime(79437600);
    TEST_ASSERT_EQUAL_UINT(state.getNextSendTime(), 79437900);
    state.setRealTime(79437601);
    TEST_ASSERT_EQUAL_UINT(state.getNextSendTime(), 79437900);
    state.setRealTime(79437900);
    TEST_ASSERT_EQUAL_UINT(state.getNextSendTime(), 79438200);
}