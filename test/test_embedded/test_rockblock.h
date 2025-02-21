#include <unity.h>
#include <rockblock.h>
#include <rockblock.cpp>


void testExtractFrame() {
    TEST_ASSERT_EQUAL_INT(1, 1);
    char testData[] = "AT\r\nOK\r\nAT+NEXT\r\nOK\r\n";
    char frame[100] = {0};
    extractFrame(frame, testData);
    TEST_ASSERT_EQUAL_STRING("AT\r\nOK\r\n", frame);
    TEST_ASSERT_EQUAL_STRING("AT+NEXT\r\nOK\r\n", testData);
}

void testParseFrame() {
    // two line frame
    char testData[] = "AT\r\nOK\r\n";
    FrameParser parser = FrameParser();
    parser.parse(testData);
    TEST_ASSERT_EQUAL_INT16(OK_STATUS, parser.status);
    TEST_ASSERT_EQUAL_STRING("AT", parser.command);
    TEST_ASSERT_EQUAL_STRING("", parser.response);
    // three line frame
    char testData2[] = "AT+CSQ\r\n+CSQ:0\r\nOK\r\n";
    parser.parse(testData2);
    TEST_ASSERT_EQUAL_INT16(OK_STATUS, parser.status);
    TEST_ASSERT_EQUAL_STRING("AT+CSQ", parser.command);
    TEST_ASSERT_EQUAL_STRING("+CSQ:0", parser.response);
    // test weird frame
    char testData3[] = "Not a valid frame\r\n";
    parser.parse(testData3);
    TEST_ASSERT_EQUAL_INT16(WAIT_STATUS, parser.status);
    TEST_ASSERT_EQUAL_STRING("Not a valid frame", parser.command);
    TEST_ASSERT_EQUAL_STRING("", parser.response);
    // test empty frame
    char testData4[] = "";
    parser.parse(testData4);
    TEST_ASSERT_EQUAL_INT16(WAIT_STATUS, parser.status);
    TEST_ASSERT_EQUAL_STRING("", parser.command);
    TEST_ASSERT_EQUAL_STRING("", parser.response);
}