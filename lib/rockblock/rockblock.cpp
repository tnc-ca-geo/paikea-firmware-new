#include <rockblock.h>

const char* OK_TOKEN = "\r\nOK\r\n";
const char* ERROR_TOKEN = "\r\nERROR\r\n";
const char* READY_TOKEN = "\r\nREADY\r\n";

const char* SBDWT_COMMAND = "AT+SBDWT\r";
const char* SBDIX_COMMAND = "AT+SBDIX\r";

// Clear MO buffer
const char* SBDD_COMMAND = "AT+SBDD0\r";

// Status type
enum Status { WAIT_STATUS, OK_STATUS, READY_STATUS, ERROR_STATUS };


size_t find(char* bfr, size_t maxLen, const char* token, size_t tokenLen) {
    size_t idx = 0;
    bool found = true;
    while (bfr[idx] != 0 && idx < maxLen - tokenLen) {
        found = true;
        for (size_t i=0; i<tokenLen; i++) {
            if (bfr[idx+i] != token[i]) {
                found = false;
                break;
            }
        }
        if (found) { return idx; }
        idx++;
    }
    return -1;
}

/*
 * Extract a frame (as a parsing unit) by token and remove it from the incoming
 * serialBuffer. bfr[0] will be 0 if no frame extracted.
 */
void extractFrameByToken(char* bfr, char* serialBuffer, const char* token) {
    size_t offset = find(serialBuffer, 1000, token, strlen(token));
    size_t len = 0;
    if (offset != -1) {
        // add plus 1 because offset is an index (starting at 0) but we need
        // count
        memcpy(bfr, serialBuffer, offset + strlen(token) + 1);
        len = strlen(serialBuffer);
        memcpy(serialBuffer, serialBuffer + offset + strlen(token), len);
        serialBuffer[len] = 0;
    } else {
        bfr = {0};
    }
}

/*
 * Initialize Rockblock instance by passing IO expander and HardwareSerial
 * reference.
 */
Rockblock::Rockblock(Expander &expander, HardwareSerial &serial) {
    this->expander = &expander;
    this->serial = &serial;
}

bool Rockblock::available() {
    return this->enabled && !this->queued && !this->sending;
}

void Rockblock::beginJoin() {}

bool Rockblock::configure() {
    return true;
}

void Rockblock::sendMessage(char *bfr, size_t len) {
    Serial.println("Queuing message");
    snprintf(this->message, 340, "%s\r", bfr);
    this->serial->print(SBDWT_COMMAND);
    this->queued = true;
};

/*
 * Full AT serial roundtrip, use to set configuration values.
 */
void Rockblock::sendAT(char *command, char *bfr) {
    this->serial->print(command);
    // TODO: The module need some time to fully respond, create in a more async
    // manner since even 300ms are not enough for a full roundtrip to the
    // satellite or timeout
    // vTaskDelay( pdMS_TO_TICKS( 1000 ));
    this->readResponse(bfr);
}

/*
 * Read Serial Response
 */
void Rockblock::readResponse(char *bfr) {
    size_t idx = 0;
    while(this->serial->available()) {
        bfr[idx] = this->serial->read();
        if (idx > 254) {
            Serial.println("Read buffer overflow!");
            break;
        } else idx++;
    }
}

void Rockblock::toggle(bool on) {
    Serial.print("Toggle Rockblock ");
    Serial.println(on ? "ON" : "OFF");
    this->expander->pinMode(13, EXPANDER_OUTPUT);
    this->expander->digitalWrite(13, !on);
    // We issue a command to receive at least one OK before reading additional
    // data. We are using a command that does not have any influence on the
    // state other than setting enabled on.
    this->serial->print("AT\r");
    // this->enabled = on;
}

/*
 * Parsing the Serial stream, with the goal to populate state variables
 * depending on response.
 * We have two levels of tokenizition, line breaks, and two or three lines
 * consisting the response to a single command. Roughly, repeated command,
 * values, OK or ERROR. For this reason we have to find the beginning.
 */
void Rockblock::parseResponse() {
    // Using a bigger buffer for now assuming that we could have several
    // messages in buffer
    char bfr[255] = {0};
    Status status = WAIT_STATUS;
    char frame[255] = {0};
    size_t offset = 0;
    size_t len = 0;
    this->readResponse(bfr);
    memcpy(this->stream + strlen(this->stream), bfr, strlen(bfr));

    // frame should be empty
    if (frame[0] == 0) {
        extractFrameByToken(frame, this->stream, OK_TOKEN);
        status = (frame[0] != 0) ? OK_STATUS : WAIT_STATUS;
    }
    // check for error frame
    if (frame[0] == 0) {
        extractFrameByToken(frame, this->stream, ERROR_TOKEN);
        status = (frame[0] != 0) ? ERROR_STATUS : WAIT_STATUS;
    }
    // chech for ready frame
    if (frame[0] == 0) {
        extractFrameByToken(frame, this->stream, READY_TOKEN);
        status = (frame[0] != 0) ? READY_STATUS : WAIT_STATUS;
    }

    if (frame[0]) {
        Serial.print("FRAME:\r\n"); Serial.println(frame);
        Serial.print("STATUS: "); Serial.println(status);
    }

    if (status == WAIT_STATUS) {
        return;
    }

    if (status == READY_STATUS) {
        // initialize sending
        this->serial->print(this->message);
        this->messageWaiting = true;
        this->nextTry = esp_timer_get_time() / 1e6;
        return;
    }

    if (status == OK_STATUS) {
        Serial.println("OK STATUS");
        // Check for beginning message.
        if (find(frame, 10, "AT\r\r", 4) == 0) {
            Serial.println("Rockblock ready!");
            this->enabled = true;
        }
        // Check for send success
        if (find(frame, 10, SBDIX_COMMAND, strlen(SBDIX_COMMAND)) == 0) {
            Serial.println("Sending response");
        }
    }

    // Check for queued message in MO and attempt sending if retry time is up.
    if (this->messageWaiting && nextTry < this -> internalTime) {
        this->serial->print(SBDIX_COMMAND);
        this->nextTry += 5;
        this->sending = true;
    }

}

void Rockblock::loop(int64_t time) {
    this -> internalTime = time / 1E6;
    this -> parseResponse();
}
