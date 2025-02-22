#ifndef __ROCKBLOCK_H__
#define __ROCKBLOCK_H__
#include <Arduino.h>
#include <tca95xx.h>
#include <hal.h>
#include <vector>

#define MAX_COMMAND_SIZE 100
#define MAX_RESPONSE_SIZE 100
#define MAX_MESSAGE_SIZE 340

// Rockblock status type
enum RockblockStatus { WAIT_STATUS, OK_STATUS, READY_STATUS, ERROR_STATUS };
// State machine type
enum StateMachine {
    IDLE, WAITING_FOR_INPUT, ATTEMPT_SENDING, MESSAFE_IN_INBOX };


class FrameParser {
    private:
        void parseResponse(const char *line);
    public:
        FrameParser() {};
        char command[MAX_COMMAND_SIZE] = {0};
        char response[MAX_RESPONSE_SIZE] = {0};
        RockblockStatus status = WAIT_STATUS;
        std::vector<int16_t> values;
        void parse(const char *frame);
};


class Rockblock {

    private:
        AbstractSerial* serial;
        AbstractExpander* expander;
        FrameParser parser = FrameParser();
        char message[MAX_MESSAGE_SIZE] = {0};
        // buffer of unhandled serial data
        char stream[1000] = {0};
        // pointer to stream_index address
        uint16_t streamIdx = 0;
        bool queued = false;
        bool sending = false;
        bool messageWaiting = false;
        bool commandWaiting = false;
        time_t nextTry = 0;
        time_t internalTime = 0;
        void readAndAppendResponse();
        void parseResponse();
        void sendCommand(const char *command);
        void parseSbdixResponse(const char *frame);
        int8_t parseCsqResponse(const char *frame);

    public:
        Rockblock(AbstractExpander &expander, AbstractSerial &serial);
        bool enabled;
        bool available();
        void emptyQueue();
        void sendMessage(char *buffer, size_t len=255);
        void toggle(bool on=false);
        // process loop, we passing in the timer for better testibility,
        void loop(int64_t time);
};

#endif