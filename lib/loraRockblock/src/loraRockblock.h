/*
 * This module simulates the Rockblock using a M5 LoraWan ASR6501 module. It is
 * not really a great LoRaWAN implementation.
 */
#ifndef __LORAROCKBLOCK_H__
#define __LORAROCKBLOCK_H__
#include <Arduino.h>
#include <tca95xx.h>

#define NOEVENT 0
#define JOIN_SUCCESS 1
#define JOIN_FAILURE 2
#define SEND_SUCCESS 3
#define SEND_FAILURE 4

/*
 * Borrowing from
 * https://github.com/m5stack/M5Stack/blob/master/examples/Modules/COM_LoRaWAN915/COM_LoRaWAN915.ino
 */

class LoraRockblock {
    private:
        // Hardware pointers
        HardwareSerial* serial;
        Expander *expander;
        // State variables
        bool enabled = false;
        int16_t lastDr = 0;
        char lastMessage[255] = {0};
        int16_t lastRssi = 0;
        int16_t lastSnr = 0;
        // ----- Private methods -----
        /* Compose a message AT command including encoding */
        void parseMessage(char *bfr);
        void readResponse(char *buffer);
        void sendAT(char *command, char *bfr);
        bool sendAndCheckAT(char *command, char *bfr, char *expected);
        // new status variables
        uint32_t status = 0;
        bool joinOk = false;
        bool joinFailure = false;
        bool sendSuccess = false;
        // new methods
        // read serial and parse state
        void readSerial();
        void parseState(char *buffer);
        // update state logic by overwritting state variables
        // when newer information available
        void updateStateLogic();

    public:
        LoraRockblock(Expander &expander, HardwareSerial &serial);
        /* Check whether we can initiate commands */
        bool available();
        void beginJoin();
        bool configure();
        int32_t getRssi();
        size_t getLastMessage(char *bfr);
        bool getSendSuccess();
        void loop();
        void sendMessage(char *buffer, size_t len=255);
        void toggle(bool on);
};

#endif /* __LORAROCKBLOCK_H__ */
