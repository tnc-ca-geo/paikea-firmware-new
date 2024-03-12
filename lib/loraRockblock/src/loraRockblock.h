/*
 * This module simulates the Rockblock using a M5 LoraWan ASR6501 module. It is
 * not really a great LoRaWAN implementation.
 */
#ifndef __LORAROCKBLOCK__
#define __LORAROCKBLOCK__
#include <Arduino.h>
#include <tca95xx.h>


class LoraRockblock {
    private:
        HardwareSerial* serial;
        Expander *expander;
        bool enabled = false;
    public:
        LoraRockblock(Expander &expander, HardwareSerial &serial);
        void enable();
        void disable();
        void loop();
        void toggle(bool on);
};

#endif
