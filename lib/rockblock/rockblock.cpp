#include <rockblock.h>
#include <cstring>
#include <vector>

#define MAX_FRAME_SIZE 255
#define OK_TOKEN "OK"
#define ERROR_TOKEN "ERROR"
#define READY_TOKEN "READY"

// This unfortunately requires the Arduino String class to be printable.
std::map<RockblockStatus, String> statusLabels = {
    {WAIT_STATUS, "WAIT"}, {OK_STATUS, "OK"}, {READY_STATUS, "READY"},
    {ERROR_STATUS, "ERROR"}
};

/*
 * Those are the implemented Rockblock commands
 */
// Basic AT interaction
const char* AT_COMMAND = "AT\r\n";
// Create a prompt to store text message
const char* SBDWT_COMMAND = "AT+SBDWT\r\n";
// We could extent on the SBDIX command since it would take a location as
// argument which might save payload size.
const char* SBDIX_COMMAND = "AT+SBDIX\r\n";
// measure signal strength command
const char* CSQ_COMMAND = "AT+CSQ\r\n";
// clear MO buffer command
const char* SBDD_COMMAND = "AT+SBDD2\r\n";

/*
 * Extract a frame by token and remove it from the incoming serialBuffer.
 * bfr[0] will be 0 if no frame extracted.
 */
void extractFrame(char* bfr, char* serialBuffer) {
    const char *stringConstants[] = {OK_TOKEN, ERROR_TOKEN, READY_TOKEN};
    const char* pos = nullptr;
    size_t len = 0;
    size_t idx = 0;
    for (idx = 0; idx < 3; idx++) {
        pos = strstr(serialBuffer, stringConstants[idx]);
        if (pos != nullptr) { break; }
    }
    if (pos == nullptr) {
        bfr[0] = '\0';
        return;
    }
    len = pos - serialBuffer + strlen(stringConstants[idx]) + 2;
    memcpy(bfr, serialBuffer, len);
    strcpy(serialBuffer, pos + strlen(stringConstants[idx]) + 2);
}

/*
 * Parses a response line from the Rockblock device and extracts integer values.
 */
void FrameParser::parseResponse(const char* line) {
    char* rest_ptr = nullptr;
    char copy_of_line[MAX_FRAME_SIZE] = {0};
    std::strncpy(copy_of_line, line, MAX_FRAME_SIZE-1);
    char* token = strtok_r(copy_of_line, ":", &rest_ptr);
    token = strtok_r(nullptr, ",", &rest_ptr);
    this->values.clear();
    while (token != nullptr) {
        this->values.push_back(std::stoi(token));
        token = strtok_r(nullptr, ",", &rest_ptr);
    }
}

/*
 * Parse a Rockblock Serial frame
 */
void FrameParser::parse(const char *frame) {
    // strtok will consume the buffer, using a copy for now
    // TODO: optimize once we don't need buffer elsewhere
    char* rest_ptr = nullptr;
    char copy_of_frame[MAX_FRAME_SIZE] = {0};
    std::strncpy(copy_of_frame, frame, MAX_FRAME_SIZE);
    // use strtok_r since we have nested calls
    char* token = strtok_r(copy_of_frame, "\r\n", &rest_ptr);
    size_t idx = 0;
    // re-setting default values, i.e. \0 and WAIT_STATUS
    this->status = WAIT_STATUS;
    memset(this->command, 0, MAX_COMMAND_SIZE);
    memset(this->response, 0, MAX_RESPONSE_SIZE);
    // parse lines for initial command,
    // response, and status
    while (token != nullptr) {
        // get command
        if (idx==0) {
            strncpy(this->command, token, MAX_COMMAND_SIZE);
        }
        // parse response if available
        if (idx==1) {
            if  (token[0] == '+') {
                memset(this->response, 0, MAX_RESPONSE_SIZE);
                strncpy(this->response, token, MAX_RESPONSE_SIZE);
                this->parseResponse(token);
            } else {
                this->response[0] = '\0';
            }
        }
        // Get status, occurs on the 2nd or 3rd line depending on response
        if (idx > 0) {
            if (strstr(token, OK_TOKEN) != nullptr) {
                this->status = OK_STATUS;
            } else if (strstr(token, ERROR_TOKEN) != nullptr) {
                this->status = ERROR_STATUS;
            } else if (strstr(token, READY_TOKEN) != nullptr) {
                this->status = READY_STATUS;
            } else {
                this->status = WAIT_STATUS;
            }
        }
        // get next token
        token = strtok_r(nullptr, "\r\n", &rest_ptr);
        idx++;
    }
}

/*
 * Initialize Rockblock instance by passing IO expander and HardwareSerial
 * reference.
 */
Rockblock::Rockblock(AbstractExpander &expander, AbstractSerial &serial) {
    this->expander = &expander;
    this->serial = &serial;
}

/*
 * Send command and wait for response or (timeout)
 */
void Rockblock::sendCommand(const char *command) {
    Serial.print("\nSend Command: "); Serial.println(command);
    if (!this->commandWaiting) {
        this->serial->print(command);
        this->commandWaiting = true;
    } else {
        Serial.println("DEBUG: Command already waiting");
    }
}

/*
 * Queue a message to send
 */
void Rockblock::sendMessage(char *bfr, size_t len) {
    Serial.println("Queueing message");
    memset(this->message, 0, MAX_MESSAGE_SIZE);
    snprintf(this->message, 340, "%s\r", bfr);
    this->queued = true;
    this->sendSuccess = false;
};

/*
 * Turn Rockblock on before sending and turn it off before sleep
 */
void Rockblock::toggle(bool on) {
    this->expander->pinMode(13, EXPANDER_OUTPUT);
    this->expander->digitalWrite(13, !on);
    this->on = on;
}

/*
 * Read the serial and add to this->stream
 */
void Rockblock::readAndAppendResponse() {
    char bfr[255] = {0};
    size_t idx = 0;
    size_t remainingSpace = sizeof(this->stream) - strlen(this->stream) - 1;
    while(this->serial->available()) {
        bfr[idx] = this->serial->read();
        if (idx > 253) { break; }
        else idx++;
    }
    strncat(this->stream, bfr, remainingSpace);
}

/*
 * We are using a state machine to parse the incoming serial data.
 * There are two levels of tokenizition: line breaks and two or three lines
 * forming a command response (frame).
 */
void Rockblock::loop() {

    // Assume WAIT_STATUS if no other status can be parsed
    RockblockStatus status = WAIT_STATUS;
    char frame[255] = {0};

    // Rockblock is not able to respond in this state, no matter from which
    // state we are coming; avoid communication errors.
    if (this->on == 0) { this->state == OFFLINE; }

    // copy latest incoming serial data to this->stream
    this->readAndAppendResponse();
    // extract a frame from this->stream
    extractFrame(frame, this->stream);
    // parse the frame
    this->parser.parse(frame);

    // clear command waiting if OK status
    if (parser.status == OK_STATUS || parser.status == READY_STATUS) {
        this->commandWaiting = false;
    }

    // Some debug output
    if (frame[0] != 0) {
        Serial.print("Last command: "); Serial.println(this->parser.command);
        Serial.print("Last response: "); Serial.println(this->parser.response);
        Serial.print("Status: ");
        Serial.println(statusLabels[this->parser.status]);
        /* Serial.print("Values: ");
        for (size_t i = 0; i < this->parser.values.size(); i++) {
            Serial.print(this->parser.values[i]); Serial.print(", ");
        }
        Serial.println();*/
    }

    // Update state
    switch(this->state) {

        case OFFLINE:
            if (this->on) {
                if (parser.status == WAIT_STATUS && !this->commandWaiting) {
                    // Check whether Rockblock is available and clear MO and MT
                    // buffers
                    sendCommand(SBDD_COMMAND);
                } else if (parser.status == OK_STATUS) {
                    this->state = IDLE;
                }
            break;

        case IDLE:
            if (
                parser.status == WAIT_STATUS && !this->commandWaiting &&
                this->queued
            ) {
                sendCommand(SBDWT_COMMAND);
                this->state = MESSAGE_WAITING;
            };
            break;

        case MESSAGE_WAITING:
            if (parser.status == READY_STATUS) {
                Serial.println("Rockblock ready for text input: ");
                this->serial->print(this->message);
            } else if (parser.status == OK_STATUS) {
                this->state = COM_CHECK;
            }
            break;

        case COM_CHECK:
            if (parser.status == WAIT_STATUS && !this->commandWaiting) {
                sendCommand(CSQ_COMMAND);
            } else if (
                parser.status == OK_STATUS &&
                strstr(this->parser.command, "CSQ") != nullptr
            ) {
                Serial.print("Signal strength: ");
                Serial.println(this->parser.values[0]);
                if (this->parser.values[0] > 2) {
                    Serial.println("Send attempt.");
                    this->state = SENDING;
                } else {
                    Serial.println("Signal strength too low to send.");
                }
            }
            break;

        case SENDING:
            if (parser.status == WAIT_STATUS && !this->commandWaiting) {
                sendCommand(SBDIX_COMMAND);
            } else if (
                parser.status == OK_STATUS &&
                strstr(this->parser.command, "SBDIX") != nullptr
            ) {
                if (this->parser.values[0] < 5) {
                    Serial.println("Send success.");
                    this->sendSuccess = true;
                    this->queued = false;
                    this->state = IDLE;
                } else {
                    Serial.println("Send failed.");
                    this->state = COM_CHECK;
                }
            }
            break;
        }
    }
}
