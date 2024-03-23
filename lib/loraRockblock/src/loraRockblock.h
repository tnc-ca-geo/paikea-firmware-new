/*
 * This module simulates the Rockblock using a M5 LoraWan ASR6501 module. It is
 * not really a great LoRaWAN implementation.
 */
#ifndef __LORAROCKBLOCK__
#define __LORAROCKBLOCK__
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
        bool commandWaiting = false;
        bool enabled = false;
        uint8_t event = NOEVENT;
        bool joining = false;
        int16_t lastDr = 0;
        char lastMessage[255] = {0};
        int16_t lastRssi = 0;
        int16_t lastSnr = 0;
        bool messageInQueue = false;
        char nextCommand[512] = {0};
        char outGoingMessage[255] = {0};
        bool sending = false;
        // ----- Private methods -----
        /* Check whether we can initiate commands */
        bool available();
        /* Compose a message AT command including encoding */
        void createMessageCommand();
        bool matchBuffer(char *haystack, char *needle, size_t len=255);
        size_t parseMessage(char *bfr);
        void parseResponse(char *bfr);
        void readResponse(char *buffer);
        char responseBuffer[255] = {0};
        bool sendAndReceive(char *command, char *bfr);
        bool sendAndReceive(char *command, char *bfr, char *expected);
        bool sendSuccess = false;
    public:
        LoraRockblock(Expander &expander, HardwareSerial &serial);
        void join();
        bool configure();
        bool getEnabled();
        int32_t getRssi();
        size_t getLastMessage(char *bfr);
        bool getSendSuccess();
        void loop();
        // Use to send message in main program
        bool queueMessage(char *buffer, size_t len=255);
        void toggle(bool on);
};

#endif
