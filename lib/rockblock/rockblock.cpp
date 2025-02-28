#include <rockblock.h>
#include <cstring>
#include <vector>

#define MAX_FRAME_SIZE 255
#define OK_TOKEN "OK"
#define ERROR_TOKEN "ERROR"
#define READY_TOKEN "READY"
#define SEND_THRESHOLD 2
#define LINE_SEP "\r\n"

// This unfortunately requires the Arduino String class to be printable.
std::map<RockblockStatus, String> statusLabels = {
    {WAIT_STATUS, "WAIT"}, {OK_STATUS, "OK"}, {READY_STATUS, "READY"},
    {ERROR_STATUS, "ERROR"}
};

/*
 * Implemented Rockblock commands, see
 * file:///Users/falk.schuetzenmeister/Downloads/Iridium%20ISU%20AT%20Command%20Reference%20v5%20(1).pdf
 */
// Basic AT interaction, will be extended to AT\r\n
const char* AT_COMMAND = "";
// Create a prompt to store text message
const char* SBDWT_COMMAND = "SBDWT";
// We could extent on the SBDIX command since it would take a location as
// argument which might save on payload size.
const char* SBDIX_COMMAND = "SBDIX";
// signal strength command
const char* CSQ_COMMAND = "CSQ";
// clear MO buffer command
const char* SBDD_COMMAND = "SBDD2";
// retrieve incoming message
const char* SBDRT_COMMAND = "SBDRT";

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
 * Parse a Rockblock Serial frame. strtok does unfortunately not work with
 * empty tokens, however text can contain two LINE_SEP in a row with nothing
 * inbetween.
 */
void FrameParser::parse(const char *frame) {
    // strtok will consume the buffer, using a copy for now
    // TODO: optimize once we don't need buffer elsewhere
    char* rest_ptr = nullptr;
    char copy_of_frame[MAX_FRAME_SIZE] = {0};
    // payload can stretch multiple lines
    size_t payload_idx = 0;
    std::strncpy(copy_of_frame, frame, MAX_FRAME_SIZE);
    // use strtok_r since we have nested calls
    char* token = strtok_r(copy_of_frame, LINE_SEP, &rest_ptr);
    size_t idx = 0;
    // re-setting default values, i.e. \0 and WAIT_STATUS
    this->status = WAIT_STATUS;
    memset(this->command, 0, MAX_COMMAND_SIZE);
    memset(this->response, 0, MAX_RESPONSE_SIZE);
    memset(this->payload, 0, MAX_MESSAGE_SIZE);
    // Parse lines for initial command, response, and status.
    // we have to be aware of the fact that the payload can stretch multiple
    // lines. But will always follow the command line.
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
        // Get status, occurs on last line, on the second at the earliest
        if (idx > 0) {
            if (strstr(token, OK_TOKEN) != nullptr) {
                this->status = OK_STATUS;
                break;
            } else if (strstr(token, ERROR_TOKEN) != nullptr) {
                this->status = ERROR_STATUS;
                break;
            } else if (strstr(token, READY_TOKEN) != nullptr) {
                this->status = READY_STATUS;
                break;
            }
        }
        // Get payload, i.e. incoming messages
        if (idx > 1) {
            // There are already characters in the payload buffer meaning
            // the tokenization acted on the message itself. Putting the token
            // back in.
            if (payload_idx > 0) {
                strncpy(this->payload + payload_idx, LINE_SEP, sizeof(LINE_SEP));
                payload_idx += sizeof(LINE_SEP) - 1;
            }
            strncpy(this->payload + payload_idx, token, MAX_RESPONSE_SIZE);
            payload_idx += sizeof(token) + 1;
        }
        // get next token
        token = strtok_r(nullptr, LINE_SEP, &rest_ptr);
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
    char commandBfr[10] = {0};
    snprintf(commandBfr, 10, "AT+%s\r\n", command);
    Serial.print("\nSend Command: "); Serial.println(commandBfr);
    if (!this->commandWaiting) {
        this->commandWaiting = true;
        this->serial->print(commandBfr);
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
 * Get the incoming message. Will be only available untile new message is sent.
 */
void Rockblock::getLastIncoming(char *bfr, size_t len) {};

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
    if (
        parser.status == OK_STATUS || parser.status == READY_STATUS ||
        parser.status == ERROR_STATUS)
    {
        this->commandWaiting = false;
    }

    // Some debug output
    if (frame[0] != 0) {
        Serial.println("FRAME");
        Serial.println(frame);
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

    // We need this because it is a combination of device status and user
    // workflow
    bool ready_for_command = (
        parser.status == WAIT_STATUS && !this->commandWaiting);
    // Update state
    switch(this->state) {

        case OFFLINE:
            if (this->on) {
                if (ready_for_command) {
                    // Check whether Rockblock is available and clear MO and MT
                    // buffers
                    sendCommand(SBDD_COMMAND);
                } else if (parser.status == OK_STATUS) {
                    this->state = IDLE;
                }
            }
            break;

        case IDLE:
            if (ready_for_command && this->queued) {
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
            if (ready_for_command) { sendCommand(CSQ_COMMAND); }
            else if (
                parser.status == OK_STATUS &&
                strstr(this->parser.command, CSQ_COMMAND) != nullptr
            ) {
                Serial.print("Signal strength: ");
                Serial.println(this->parser.values[0]);
                if (this->parser.values[0] > SEND_THRESHOLD) {
                    Serial.println("Attempt sending.");
                    this->state = SENDING;
                } else {
                    Serial.println("Signal strength too low to send.");
                }
            }
            break;

        case SENDING:
            if (ready_for_command) { sendCommand(SBDIX_COMMAND); }
            else if (
                parser.status == OK_STATUS &&
                strstr(this->parser.command, SBDIX_COMMAND) != nullptr
            ) {
                if (this->parser.values[0] < 5) {
                    Serial.println("Send success.");
                    this->sendSuccess = true;
                    this->queued = false;
                    // check for new incoming message
                    if (this->parser.values[2] == 1) {
                        this->state = INCOMING;
                    } else {
                        this->state = IDLE;
                    }
                } else {
                    Serial.println("Send failed.");
                    this->state = COM_CHECK;
                }
            }
            break;

        case INCOMING:
            if (ready_for_command) { sendCommand(SBDRT_COMMAND); }
            else if (
                parser.status == OK_STATUS &&
                strstr(this->parser.command, SBDRT_COMMAND) != nullptr
            ) {
                this->state = IDLE;
            }
            break;
    }
}
