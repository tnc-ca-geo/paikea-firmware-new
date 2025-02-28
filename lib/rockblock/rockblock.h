/*
 * Implements the Rockblock API partially as needed by Scout
 *
 * See https://docs.groundcontrol.com/iot/rockblock/user-manual/at-commands
 */
#ifndef __ROCKBLOCK_H__
#define __ROCKBLOCK_H__
#include <Arduino.h>
#include <tca95xx.h>
#include <hal.h>
#include <vector>
#include <map>

// TODO: Determine actual maximum sizes. There is plenty of memory though.
#define MAX_COMMAND_SIZE 100
#define MAX_RESPONSE_SIZE 100
#define MAX_MESSAGE_SIZE 340

// Rockblock status type
enum RockblockStatus { WAIT_STATUS, OK_STATUS, READY_STATUS, ERROR_STATUS };
// State machine type
enum StateMachine {
    OFFLINE, IDLE, MESSAGE_WAITING, MESSAGE_IN_RB, COM_CHECK, SENDING,
    INCOMING
};

// Parse Serial frames
class FrameParser {
    private:
        void parseResponse(const char *line);
    public:
        FrameParser() {};
        char command[MAX_COMMAND_SIZE] = {0};
        char response[MAX_RESPONSE_SIZE] = {0};
        RockblockStatus status = WAIT_STATUS;
        std::vector<int16_t> values;
        // 270 is the maxim
        char payload[MAX_MESSAGE_SIZE] = {0};
        void parse(const char *frame);
};

// Rockblock state machine
class Rockblock {

    private:
        AbstractSerial* serial;
        AbstractExpander* expander;
        FrameParser parser = FrameParser();
        char message[MAX_MESSAGE_SIZE] = {0};
        char incoming[MAX_MESSAGE_SIZE] = {0};
        // buffer of unhandled serial data
        char stream[1024] = {0};
        bool on = false;
        bool queued = false;
        bool commandWaiting = false;
        void readAndAppendResponse();
        void sendCommand(const char *command);

    public:
        Rockblock(AbstractExpander &expander, AbstractSerial &serial);
        StateMachine state = OFFLINE;
        bool sendSuccess = false;
        void sendMessage(char *buffer, size_t len=255);
        void getLastIncoming(char *buffer, size_t len=MAX_MESSAGE_SIZE);
        void toggle(bool on=false);
        // process loop, we passing in the timer for better testibility,
        void loop();
};

#endif