#include <rockblock.h>
#include <cstring>
#include <vector>

#define MAX_FRAME_SIZE 255
#define OK_TOKEN "OK"
#define ERROR_TOKEN "ERROR"
#define READY_TOKEN "READY"

const char* SBDWT_COMMAND = "AT+SBDWT\r\n";
const char* SBDIX_COMMAND = "AT+SBDIX\r\n";
// measure signal strength command
const char* CSQ_COMMAND = "AT+CSQ\r\n";
// clear MO buffer command
const char* SBDD_COMMAND = "AT+SBDD0\r\n";


/*
 * Find a token in a buffer and return the index. If not found, return -1.
 */
int16_t find(const char* bfr, size_t maxLen, const char* token) {
    const char* pos = strstr(bfr, token);
    if (pos != nullptr && (pos - bfr) < maxLen) { return pos - bfr; }
    return -1;
}

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
 * Parse a response line
 */
void FrameParser::parseResponse(const char *line) {
    char *token;
    char *rest_ptr = NULL;
    char copy_of_line[MAX_FRAME_SIZE] = {0};
    this->values.clear();
    std::strncpy(copy_of_line, line, MAX_FRAME_SIZE);
    token = strtok_r(copy_of_line, ":", &rest_ptr);
    token = strtok_r(NULL, ",", &rest_ptr);
    while (token != NULL) {
        this->values.push_back(std::stoi(token));
        token = strtok_r(NULL, ",", &rest_ptr);
    }
}

/*
 * Parse a Rockblock Serial frame
 */
void FrameParser::parse(const char *frame) {
    // strtok will consume the buffer, using a copy for now
    // TODO: optimize once we don't need buffer elsewhere
    char *token;
    char *rest_ptr = NULL;
    char copy_of_frame[MAX_FRAME_SIZE] = {0};
    std::strncpy(copy_of_frame, frame, MAX_FRAME_SIZE);
    // use strtok_r since we have nested strtok calls
    token = strtok_r(copy_of_frame, "\r\n", &rest_ptr);
    size_t idx = 0;
    // re-setting default values, i.e. \0 and WAIT_STATUS
    this->status = WAIT_STATUS;
    memset(this->command, 0, MAX_COMMAND_SIZE);
    memset(this->response, 0, MAX_RESPONSE_SIZE);
    // parse lines for initial command,
    // response, and status
    while (token != NULL) {
        // command
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
        // status which could occur on the second or third line depending
        // on the response
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
        token = strtok_r(NULL, "\r\n", &rest_ptr);
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
 * Defines when Rockblock is able to handle new messages
 */
bool Rockblock::available() {
    return this->enabled && !this->queued && !this->messageWaiting;
}

/*
 * Send command and wait for response or (timeout)
 */
void Rockblock::sendCommand(const char *command) {
    Serial.print("\n\nSend Command: "); Serial.println(command);
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
    Serial.println("Queuing message");
    snprintf(this->message, 340, "%s\r", bfr);
    sendCommand(SBDWT_COMMAND);
    // this->queued = true;
};

/*
 * Empty the queue: IMPLEMENT
 */
void Rockblock::emptyQueue() {
    sendCommand(SBDD_COMMAND);
};

void Rockblock::toggle(bool on) {
    this->enabled = false;
    this->expander->pinMode(13, EXPANDER_OUTPUT);
    this->expander->digitalWrite(13, !on);
    // We issue a command to receive at least one OK before reading additional
    // data. We are using a command that does not have any influence on the
    // state other than setting enabled on. An OK response will toggle the
    // Rocklock.
    if (on) { sendCommand("AT\r\n"); }
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

void Rockblock::parseSbdixResponse(const char* frame) {
    int8_t pos = find(frame, 255, "+SBDIX: 0,");
    if (pos > 0) {
        Serial.print("DEBUG: Send success");
        this->messageWaiting = false;
    }
}

int8_t Rockblock::parseCsqResponse(const char* frame) {
    int8_t pos = find(frame, 255, "+CSQ:");
    if (pos > 0) {
        return std::stoi(std::string(1, frame[pos + 5]));
    }
    return -1;
}

/*
 * We are using a state machine to parse the incoming serial data.
 * There are two levels of tokenizition: line breaks and two or three lines
 * forming a command response (frame).
 */
void Rockblock::parseResponse() {

    RockblockStatus status = WAIT_STATUS;
    char frame[255] = {0};

    // copy latest incoming serial data to this->stream
    this->readAndAppendResponse();
    extractFrame(frame, this->stream);
    this->parser.parse(frame);

    // Some debug output
    if (frame[0] != 0) {
        Serial.println();
        Serial.print("Last command: "); Serial.println(this->parser.command);
        Serial.print("Last response: "); Serial.println(this->parser.response);
        Serial.print("Status: "); Serial.println(this->parser.status);
        Serial.print("Values: ");
        for (size_t i = 0; i < this->parser.values.size(); i++) {
            Serial.print(this->parser.values[i]); Serial.print(", ");
        }
        Serial.println();
    }

    // READY_STATUS means that the Rockblock is ready for text input. This can
    // only be triggered by the SBDWT command. Print the text to Serial and
    // schedule sending.
    if (parser.status == READY_STATUS) {
        Serial.println("DEBUG: Queueing message.");
        // Send the message (not using the sendCommand method)
        this->serial->print(this->message);
        this->messageWaiting = true;
        this->nextTry = this->internalTime;
    }

    // In WAIT_STATUS we did not receive a new frame from serial. We just wait
    // and retry already triggered actions.
    if (parser.status == WAIT_STATUS) {
        if (
            this->messageWaiting &&
            this->nextTry < this->internalTime &&
            !this->commandWaiting
        ) {
            sendCommand(CSQ_COMMAND);
        }
    }

    if (parser.status == OK_STATUS) {
        Serial.println("OK STATUS");
        // Any ok status means that the device is enabled
        this->enabled = true;
        this->commandWaiting = false;
        // Check for signal strength
        int8_t signalStrength = parseCsqResponse(frame);
        Serial.print("Signal strength: "); Serial.println(signalStrength);
        if (signalStrength > 2) {
            Serial.println("Attempting to send message");
            // sendCommand(SBDIX_COMMAND);
        }
        // Check for send success
        parseSbdixResponse(frame);
    }

}

void Rockblock::loop(int64_t time) {
    this->internalTime = time / 1E6;
    this->parseResponse();
}
