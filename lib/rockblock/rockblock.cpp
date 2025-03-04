#include <rockblock.h>
#include <cstring>
#include <vector>

#define MAX_FRAME_SIZE 255
#define OK_TOKEN "OK"
#define ERROR_TOKEN "ERROR"
#define READY_TOKEN "READY"
#define SEND_THRESHOLD 1
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
 * Strsep for multiple character delimiters.
 */
char* strsep_multi(char** stringp, const char* delim) {
    if (*stringp == NULL) { return NULL; }
    char* start = *stringp;
    char* end = strstr(start, delim);
    if (end) {
        *end = '\0';
        *stringp = end + strlen(delim);
    } else {
        *stringp = NULL;
    }
    return start;
}

/*
 * Extract a frame by token and remove it from the incoming serialBuffer.
 * bfr[0] will be 0 if no frame extracted. TODO: We could make this slightly
 * more sophisticated in a way that we could not send messages that break the
 * frame parser. E.g. if the message itself contains characters that are
 * status tokens. Leave that for later.
 */
void extractFrame(char* bfr, char* serialBuffer) {
    const char* stringConstants[] = {OK_TOKEN, ERROR_TOKEN, READY_TOKEN};
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
 * empty tokens.
 */
void FrameParser::parse(const char* frame) {
    const uint8_t sep_len = strlen(LINE_SEP);
    // strsep_multi will consume the buffer, use a copy to allow for const
    char* rest = strdup(frame);
    char* token = nullptr;
    // payload can stretch multiple lines
    size_t pld_idx = 0;
    // identify on what line we are on
    size_t idx = 0;
    // re-setting default values
    memset(this->command, 0, MAX_COMMAND_SIZE);
    memset(this->response, 0, MAX_RESPONSE_SIZE);
    memset(this->payload, 0, MAX_MESSAGE_SIZE);
    // we need this local vars since they might change during the parse process
    // assign final values at the end
    char pld[MAX_MESSAGE_SIZE] = {0};
    this->status = WAIT_STATUS;
    // iterate over tokens
    while ( (token = strsep_multi(&rest, LINE_SEP)) != NULL ) {

        // get command. Command will be always the first valid line.
        if (idx==0) {
            strncpy(this->command, token, MAX_COMMAND_SIZE);
        }
        // parse response. Response will be always the second valid line.
        if (idx==1) {
            if  (token[0] == '+') {
                memset(this->response, 0, MAX_RESPONSE_SIZE);
                strncpy(this->response, token, MAX_RESPONSE_SIZE);
                this->parseResponse(token);
            } else {
                this->response[0] = '\0';
            }
        }

        // Parse status, occurs the last line but on the second at the earliest
        if (idx > 0) {
            if (strstr(token, OK_TOKEN) != nullptr) {
                this->status = OK_STATUS;
                continue;
            } else if (strstr(token, ERROR_TOKEN) != nullptr) {
                this->status = ERROR_STATUS;
                continue;
            } else if (strstr(token, READY_TOKEN) != nullptr) {
                this->status = READY_STATUS;
                continue;
            }
        }

        // Parse payload:
        // - Will always (!) start (!) on third valid line
        // - Will not start with /r/n
        // - After that it should be tolerant to any content
        if ( (idx == 2 && token[0] != '\0') || (idx > 2 && pld_idx > 0) ) {
            if (pld_idx > 0) {
                strncpy(pld + pld_idx, LINE_SEP, sep_len);
                pld_idx += sep_len;
            }
            strncpy(pld + pld_idx, token, MAX_MESSAGE_SIZE - pld_idx);
            pld_idx += strlen(token);
        }

        // skip empty lines, increase index unless token is empty
        if (token[0] != 0) { idx++; }
    }
    // Our current parse method will generate trailing SEP
    if (pld_idx > sep_len) { strncpy(this->payload, pld, pld_idx-sep_len); }
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
void Rockblock::sendCommand(const char* command) {
    char commandBfr[10] = {0};
    snprintf(commandBfr, 10, "AT+%s\r\n", command);
    // Serial.print("\nSend Command: "); Serial.println(commandBfr);
    // for characteristics
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
void Rockblock::sendMessage(char* bfr, size_t len) {
    Serial.println("Queueing message and deleting incoming");
    memset(this->message, 0, MAX_MESSAGE_SIZE);
    memset(this->incoming, 0, MAX_MESSAGE_SIZE);
    snprintf(this->message, 340, "%s\r", bfr);
    start_time = esp_timer_get_time() / 1E6;
    retries = 0;
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
        // Serial.println("FRAME");
        // Serial.println(frame);
        Serial.print("\nLast command: "); Serial.println(this->parser.command);
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
                Serial.print(this->parser.values[0]);
                if (this->parser.values[0] > SEND_THRESHOLD) {
                    // store actual send threshold
                    this->success = this->parser.values[0];
                    Serial.println(" -> attempt sending");
                    this->state = SENDING;
                } else {
                    Serial.println(" -> too low to send");
                }
            }
            break;

        case SENDING:
            if (ready_for_command) {
                sendCommand(SBDIX_COMMAND);
                this->retries += 1;
            }
            else if (
                parser.status == OK_STATUS &&
                strstr(this->parser.command, SBDIX_COMMAND) != nullptr
            ) {
                if (this->parser.values[0] < 5) {
                    Serial.println("Send success.");
                    this->sendSuccess = true;
                    this->queued = false;
                    Serial.print("Time in seconds ");
                    Serial.println(esp_timer_get_time()/1E6 - this->start_time);
                    Serial.print("Retries "); Serial.println(this->retries);
                    Serial.print("Trigger signal strength ");
                    Serial.println(this->success);
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
                strncpy(this->incoming, parser.payload, MAX_MESSAGE_SIZE);
                this->state = IDLE;
            }
            break;
    }
}
