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
}

void testParseFrameWeirdFrame() {
    char testData[] = "Not a valid frame\r\n";
    FrameParser parser = FrameParser();
    parser.parse(testData);
    TEST_ASSERT_EQUAL_INT16(WAIT_STATUS, parser.status);
    TEST_ASSERT_EQUAL_STRING("Not a valid frame", parser.command);
    TEST_ASSERT_EQUAL_STRING("", parser.response);
}

void testParseEmptyFrame() {
    char testData[] = "";
    FrameParser parser = FrameParser();
    parser.parse(testData);
    TEST_ASSERT_EQUAL_INT16(WAIT_STATUS, parser.status);
    TEST_ASSERT_EQUAL_STRING("", parser.command);
    TEST_ASSERT_EQUAL_STRING("", parser.response);
}

void testFrameParserUncleanStart() {
    char testData[] = "\r\n\r\nAT\r\nOK\r\n";
    FrameParser parser = FrameParser();
    parser.parse(testData);
    TEST_ASSERT_EQUAL_INT16(OK_STATUS, parser.status);
    TEST_ASSERT_EQUAL_STRING("AT", parser.command);
    TEST_ASSERT_EQUAL_STRING("", parser.response);
}

void testParseValues() {
    // this test relays on Frameparser.parse since parseValues is private
    char testData[] = "AT+CSQ\r\n+SBDIX:0,1,20,3,4,5\r\nOK\r\n";
    std::vector<int16_t> expectedValues = {0, 1, 20, 3, 4, 5};
    FrameParser parser = FrameParser();
    parser.parse(testData);
    TEST_ASSERT_EQUAL_INT16(6, parser.values.size());
    for (size_t i = 0; i < parser.values.size(); ++i) {
        TEST_ASSERT_EQUAL_INT16(expectedValues[i], parser.values[i]);
    }
}

void testPayloadParsing() {
    char testData[] = "AT+SBDRT\r\n+SBDRT:\r\npayload\r\nOK\r\n";
    FrameParser parser = FrameParser();
    parser.parse(testData);
    TEST_ASSERT_EQUAL_STRING("AT+SBDRT", parser.command);
    TEST_ASSERT_EQUAL_INT16(1, parser.status);
    TEST_ASSERT_EQUAL_STRING("payload", parser.payload);

}

void testPayloadParsingMultipleLines() {
    char testData[] = "AT+SBDRT\r\n+SBDRT:\r\nfirst\r\nsecond\r\nOK\r\n";
    FrameParser parser = FrameParser();
    parser.parse(testData);
    TEST_ASSERT_EQUAL_INT16(1, parser.status);
    TEST_ASSERT_EQUAL_STRING("first\r\nsecond", parser.payload);
}

void testPayloadParsingMultipleEmpty() {
    char testData[] =
        "AT+SBDRT\r\n+SBDRT:\r\nfirst\r\n\r\nsecond\r\n\r\nOK\r\n";
    FrameParser parser = FrameParser();
    parser.parse(testData);
    TEST_ASSERT_EQUAL_STRING("first\r\n\r\nsecond\r\n", parser.payload);
}
