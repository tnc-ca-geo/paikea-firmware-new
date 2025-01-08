#ifndef __ROCKBLOCK_H__
#define __ROCKBLOCK_H__
#include <Arduino.h>
#include <tca95xx.h>


class Rockblock {

    private:
        HardwareSerial* serial;
        Expander *expander;
        char message[340] = {0};
        // buffer of unhandled serial data
        char stream[1000] = {0};
        // pointer to stream_index address
        uint16_t streamIdx = 0;
        bool queued = false;
        bool sending = false;
        bool messageWaiting = false;
        time_t nextTry = 0;
        time_t internalTime = 0;
        void readResponse(char *bfr);
        void parseResponse();

    public:
        Rockblock(Expander &expander, HardwareSerial &serial);
        bool enabled;
        bool available();
        // TODO: remove, only required by LoRoWAN
        void beginJoin();
        // TODO: remove, only required by LoRoWAN
        bool configure();
        void sendMessage(char *buffer, size_t len=255);
        void toggle(bool on=false);
        // process loop, we passing in the timer for better testibility,
        void loop(int64_t time);
        // TODO move to private
        void sendAT(char *command, char *bfr);
};

#endif