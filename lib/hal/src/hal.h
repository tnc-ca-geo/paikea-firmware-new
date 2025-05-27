/*
 * Abstract hardware dependencies for native development
 *
 * TODO: Native development has not been fully implemented because
 * of time consuming difficulties. All tests need to run on the 
 * LilyGO board used in the project, however just the board (peripherials 
 * not required) should be sufficient for testing.
 * 
 * TODO: further abstract to any ESP32 board
 */
#ifndef __HAL_H__
#define __HAL_H__
#include <stdint.h>
#ifndef NATIVE
#include <Arduino.h>
#endif

/*
 * This is a rather sparse definition of the few serial methods used in the
 * project.
 */
class AbstractSerial {
    public:
        // AbstractSerial() {};
        virtual void begin(uint16_t serialSpeed, int serial8N1,
            uint8_t rxPin, uint8_t txPin) = 0;
        virtual void print(const char *bfr) = 0;
        virtual char read() = 0;
        virtual bool available() = 0;
};

// Don't compile if we run on native
#ifndef NATIVE
/*
 * Implements the AbstractSerial class for Scout devices
 */
class RockblockSerial : public AbstractSerial {
    private:
        HardwareSerial* serial=0;
    public:
        RockblockSerial();
        virtual void begin(uint16_t serialSpeed, int serial8N1,
            uint8_t rxPin, uint8_t txPin);
        void print(const char *bfr) override;
        char read() override;
        bool available() override;
};
#endif

#endif
