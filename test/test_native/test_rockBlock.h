/* 
 * We currently not using native tests due to many time-consuming 
 * implementation difficulties. But I leave this here as a touch point.
 */
#include <unity.h>
#include <stdint.h>
// project
#include <hal.h>
#include <tca95xx.h>
#include <rockblock.h>


class MockSerial: public AbstractSerial {
    private:
        char readBuffer[255] = {0};
        char printBuffer[255] = {0};
        bool avail = false;
        size_t idx = -1;
    public:
        // Methods of the base class used by the tested code
        char read() override {
            idx ++;
            if (idx > 254) return 0;
            return readBuffer[idx];
         }
        void begin(uint16_t serialSpeed, int serial8N1,
            uint8_t rxPin, uint8_t txPin) override {};
        void print(const char *bfr) override { snprintf(printBuffer, 255, bfr); }
        bool available() override { return true; }
        // Methods specific to this mock
        void setReadBuffer(char *bfr) { snprintf(readBuffer, 255, bfr); }
        void getPrintBuffer(char *bfr) { snprintf(bfr, 255, printBuffer); }
        void setAvailable(bool on) { avail = on; }
};


class MockExpander: public AbstractExpander {
    public:
        MockExpander() {};
        void begin(uint8_t i2c_address) override {};
        void init() override {};
        void pinMode(uint8_t pin, bool mode) override {};
        void pinMode(uint8_t port, uint8_t bit, bool mode) override {};
        void digitalWrite(uint8_t pin, bool value) override {};
        void digitalWrite(uint8_t port, uint8_t bit, bool value) override {};
        bool digitalRead(uint8_t pin) override { return true; };
        bool digitalRead(uint8_t port, uint8_t bit) override { return true; };
};


void test_test() {
    char bfr[20] = {0};
    MockSerial serial;
    MockExpander expander;
    Rockblock rb(expander, serial);
    rb.toggle(true);
    serial.setReadBuffer("Hello World!");
    TEST_ASSERT_EQUAL_CHAR(serial.read(), 'H');
};