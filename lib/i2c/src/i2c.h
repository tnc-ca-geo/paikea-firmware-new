#include <Wire.h>
/*
 * This class allows for sharing the Wire object between threads.
 */
class I2C {
    private:
        bool lock = false;
    public:
        void begin() {
            Wire.begin();
        }
};
