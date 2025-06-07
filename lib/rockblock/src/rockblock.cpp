#include <rockblock.h>
#include <cstring>
#include <vector>

#define OK_TOKEN "OK"
#define ERROR_TOKEN "ERROR"
#define READY_TOKEN "READY"
#define SEND_THRESHOLD 3
#define LINE_SEP "\r\n"
#define SEP_LEN 2

/*
 * Implemented Rockblock commands, see
 *
 * https://cdn.sparkfun.com/assets/6/d/4/c/a/RockBLOCK-9603-Developers-Guide_1.pdf
 * Iridium%20ISU%20AT%20Command%20Reference%20v5%20(1).pdf
 */
// Basic AT interaction, will be extended to AT\r\n, currently not used
#define AT_COMMAND ""
// Create a prompt to store text message
#define SBDWT_COMMAND "+SBDWT"
// We could extent on the SBDIX command since it would take a location as
// argument which might save on payload size.
#define SBDIX_COMMAND "+SBDIX"
// signal strength command
#define CSQ_COMMAND "+CSQ"
// clear MO and MT buffer command
#define SBDD_COMMAND "+SBDD2"
// retrieve incoming message
#define SBDRT_COMMAND "+SBDRT"

#define RB_SUCCESS_TEMPLATE "Rockblock send success!\nTime: %.0f seconds\nRetries: %d\nTrigger signal strength: %d\n"

// This unfortunately requires the Arduino String class in order to be
// printable.
std::map<RockblockStatus, String> statusLabels = {
  {WAIT_STATUS, "WAIT"}, {OK_STATUS, "OK"}, {READY_STATUS, "READY"},
  {ERROR_STATUS, "ERROR"}
};

/*
 * Recreates std::strsep for multiple character delimiters.
 *
 * Type-wise we are a little bit on shaky ground here since we are using 
 * std::string functionality
 * 
 * On the other hand we are reproducing a common parser pattern
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
 * Convert float to a NMEA formatted string. Could be reused in ScoutMessages
 */
size_t float2NmeaNumber(char* bfr, float val, int digits=3) {
    float whole;
    float mins = abs(std::modf(val, &whole)) * 60;
    char sign = (val < 0) ? '-' : '+';
    if (digits == 2) {
        return snprintf(bfr, 32, "%c%02.0f%06.3f", sign, abs(whole), mins);
    } else {
        return snprintf(bfr, 32, "%c%03.0f%06.3f", sign, abs(whole), mins);
    }
}

/*
 * Create a location Snippet for the SBDIX command. This is somewhat similar
 * to float2Nmea in Scout messages but the format is also different hence
 * a single function instead of reusing part of it and creating three functions
 * that had to be combined in different manners. 
 */
size_t getSbdixWithLocation(char* bfr, float lat, float lon) {
    char latBfr[32] = {0};
    char lonBfr[32] = {0};
    float2NmeaNumber(latBfr, lat, 2);
    float2NmeaNumber(lonBfr, lon, 3);
    return sniprintf(bfr, 64, "%s=%s,%s", SBDIX_COMMAND, latBfr, lonBfr);
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
    len = pos + strlen(stringConstants[idx]) + 2 - serialBuffer;
    // copy frame
    memcpy(bfr, serialBuffer, len);
    // remove frame from the head of the Serial buffer
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
                strncpy(pld + pld_idx, LINE_SEP, SEP_LEN);
                pld_idx += SEP_LEN;
            }
            strncpy(pld + pld_idx, token, MAX_MESSAGE_SIZE - pld_idx);
            pld_idx += strlen(token);
        }

        // skip empty lines, increase index unless token is empty
        if (token[0] != 0) { idx++; }
    }
    // Our current parse method will generate trailing SEP
    if (pld_idx > SEP_LEN) { strncpy(this->payload, pld, pld_idx-SEP_LEN); }
    // avoid memory leak from strdup
    // https://en.cppreference.com/w/c/experimental/dynamic/strdup
    free(rest);
}

/*
 * Initialize Rockblock instance by passing IO expander and HardwareSerial
 * reference.
 */
Rockblock::Rockblock(AbstractExpander &expander,
    AbstractSerial &serial, uint8_t enable_pin
) {
    this->expander = &expander;
    this->serial = &serial;
    this->enable_pin = enable_pin;
}

/*
 * Send command and wait for response or (timeout)
 */
void Rockblock::sendCommand(const char* command) {
    char commandBfr[32] = {0};
    snprintf(commandBfr, 32, "AT%s\r\n", command);
    if (!this->commandWaiting) {
        this->commandWaiting = true;
        this->serial->print(commandBfr);
    } else {
        Serial.println("Failure. Command in process.");
    }
}

/*
 * Queue a message to send
 */
void Rockblock::sendMessage(char* bfr, size_t len) {
    Serial.println("Queue message and delete incoming");
    memset(this->message, 0, MAX_MESSAGE_SIZE);
    memset(this->incoming, 0, MAX_MESSAGE_SIZE);
    memset(this->sbidxCommand, 0, 64);
    this->start_time = esp_timer_get_time() / 1E6;
    this->retries = 0;
    this->queued = true;
    this->sendSuccess = false;
    this->locationAvailable = false;
    strncpy(this->sbidxCommand, SBDIX_COMMAND, sizeof(SBDIX_COMMAND));
    snprintf(this->message, 340, "%s\r", bfr);
};

/*
 * Queue a message to send with location
 */
void Rockblock::sendMessage(char *bfr, float lat, float lon, size_t len) {
    this->sendMessage(bfr, len);
    this->locationAvailable = true;
    getSbdixWithLocation(this->sbidxCommand, lat, lat);
}; 

/*
 * Get an incoming message. Incoming message is only available until 
 * .sendMessage() is called.
 */
void Rockblock::getLastIncoming(char *bfr, size_t len) {
    strncpy(bfr, this->incoming, MAX_MESSAGE_SIZE-1);
};

/*
 * Public getter for signal strength
 */
uint8_t Rockblock::getSignalStrength() {
    return this->signal;
}

/*
 * Turn Rockblock on before sending and turn off before sleeping
 */
void Rockblock::toggle(bool on) {
    this->expander->pinMode(this->enable_pin, EXPANDER_OUTPUT);
    this->expander->digitalWrite(this->enable_pin, !on);
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
void Rockblock::run() {

    // Assume WAIT_STATUS if parser does not return other
    RockblockStatus status = WAIT_STATUS;
    char frame[MAX_FRAME_SIZE] = {0};
    // for debug output
    char bfr[255] = {0};

    // copy latest incoming serial data to this->stream
    this->readAndAppendResponse();
    // extract a frame from this->stream
    extractFrame(frame, this->stream);
    // parse the frame
    this->parser.parse(frame);

    // clear command waiting if OK, READY, or ERROR status
    if (
        parser.status == OK_STATUS || parser.status == READY_STATUS ||
        parser.status == ERROR_STATUS)
    {
        this->commandWaiting = false;
    }

    // Add more error handling, however there is not much to do, error
    // indicates that a command was not issus correctly in the sense of
    // a syntax or value error. Our code would be solely reponsible for this.
    if (parser.status == ERROR_STATUS) {
        Serial.println("Rockblock error");
        return;
    }

    // some debug output
    if (frame[0] != 0) {
        Serial.print("\nLast command: "); Serial.println(this->parser.command);
        Serial.print("Last response: "); Serial.println(this->parser.response);
        Serial.print("Status: ");
        Serial.println(statusLabels[this->parser.status]);
    }

    // This is a combination of device status and user workflow
    bool readyForCommand = (
        parser.status == WAIT_STATUS && !this->commandWaiting);

    // Update state
    switch(this->state) {

        case OFFLINE:
            if (this->on) {
                if (readyForCommand) {
                    // Check whether Rockblock is available and clear MO and MT
                    // buffers
                    sendCommand(SBDD_COMMAND);
                } else if (parser.status == OK_STATUS) {
                    this->state = IDLE;
                }
            }
            break;

        case IDLE:
            if (readyForCommand && this->queued) {
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
            if (readyForCommand) { sendCommand(CSQ_COMMAND); }
            else if (
                parser.status == OK_STATUS &&
                strstr(this->parser.command, CSQ_COMMAND) != nullptr
            ) {
                this->signal = this->parser.values[0];
                Serial.print("Signal strength: "); Serial.print(this->signal);
                if (this->parser.values[0] >= SEND_THRESHOLD) {
                    Serial.println(" -> attempt sending");
                    this->state = SENDING;
                } else {
                    Serial.println(" -> too low to send");
                }
            }
            break;

        case SENDING:
            if (readyForCommand) {
                sendCommand(this->sbidxCommand);
                this->retries += 1;
            }
            else if (
                parser.status == OK_STATUS &&
                strstr(this->parser.command, SBDIX_COMMAND) != nullptr
            ) {
                if (this->parser.values[0] < 5) {
                    this->queued = false;
                    // check for incoming message
                    if (this->parser.values[2] == 1) {
                        Serial.println("Message waiting");
                        this->state = INCOMING;
                    } else {
                        this->state = IDLE;
                        this->sendSuccess = true;
                    }
                } else {
                    Serial.println("Send failed.");
                    this->state = COM_CHECK;
                }
            }
            break;

        case INCOMING:
            if (readyForCommand) { sendCommand(SBDRT_COMMAND); }
            else if (
                parser.status == OK_STATUS &&
                strstr(this->parser.command, SBDRT_COMMAND) != nullptr
            ) {
                strncpy(this->incoming, parser.payload, MAX_MESSAGE_SIZE);
                this->state = IDLE;
                this->sendSuccess = true;
            }
            break;
    }
}


void Rockblock::loop() {
    if (this->on) { this->run(); }
}