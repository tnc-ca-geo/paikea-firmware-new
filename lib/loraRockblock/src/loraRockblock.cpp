#include <loraRockblock.h>

#define APP_EUI "FFFFFFFFFFFF0002"
#define DEV_EUI "FFFFFFFFFFFF0002"
#define APP_KEY "D44245A81B1220776D628D03095CA7E9"

#define DEBUG 1


/*
 * TODO: Improve, check whether next character is a digit.
 */
uint16_t parseValue(char *bfr, char *valuePrefix, size_t len, char end) {
    char valueString[5] = {0};
    size_t idx = 0;
    char* pos = strstr(bfr, valuePrefix);
    if (pos != NULL) {
        pos = pos + len;
        while (pos[idx] != end) {
            valueString[idx] = pos[idx];
            idx++;
        }
        return strtol(valueString, NULL, 10);
    }
    return 0;
}

/*
 * Match two buffers, used to parse Serial return.
 */
bool LoraRockblock::matchBuffer(char* bfr, char* needle, size_t len) {
    char *s;
    s = strstr(bfr, needle);
    if ( strstr(bfr, needle) != NULL ) return true;
    return false;
}


LoraRockblock::LoraRockblock(Expander &expander, HardwareSerial &serial) {
    this->expander = &expander;
    this->serial = &serial;
}

/*
 * Full AT serial roundtrip, use to set configuration values.
 */
bool LoraRockblock::sendAndReceive(char *command, char *bfr) {
    if (this->available()) {
        this->serial->write(command);
        Serial.print(command);
        vTaskDelay( pdMS_TO_TICKS( 50 ));
        this->readResponse(bfr);
        Serial.println(bfr);
        return true;
    }
    return false;
}

/*
 * Use to set configuration values, will return true if expected response is in
 * response.
 */
bool LoraRockblock::sendAndReceive(char *command, char *bfr, char *expected) {
    // TODO: maybe use a shared response buffer and available flags
    bool res = this->sendAndReceive(command, bfr);
    Serial.print(bfr);
    if (res) return matchBuffer(bfr, expected);
    return false;
}

/*
 * Configure the modem, currently hard-wired for TTN
 */
bool LoraRockblock::configure() {
    char command[64] = {0};
    char bfr[255] = {0};
    bool res = false;
    // restore defaults
    snprintf(command, 64, "AT+CRESTORE\r");
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CCLASS=0\r");
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CRXP=0,8,923300000\r");
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CADR=1\r");
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CJOINMODE=0\r");
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CDEVEUI=%s\r", DEV_EUI);
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CAPPEUI=%s\r", APP_EUI);
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CAPPKEY=%s\r", APP_KEY);
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CFREQBANDMASK=0002\r");
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CULDLMODE=2\r");
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CSAVE\r");
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+IREBOOT=0\r");
    if (!this->sendAndReceive(command, bfr, (char*) "OK")) return false;
    vTaskDelay( pdMS_TO_TICKS(500) );
    return true;
}


void LoraRockblock::join() {
    if(this->available()) {
        snprintf(this->nextCommand, 255, "AT+CJOIN=1,1,8,16\r");
        this->joining=true;
        this->commandWaiting=true;
    } else {
        Serial.println("Lora busy. Cannot join.");
    }
}


bool LoraRockblock::queueMessage(char *bfr, size_t len) {
    size_t i=0;
    if (!messageInQueue) {
        Serial.println("Message queued!");
        for (i; i<len; i++) {
            this->outGoingMessage[i] = bfr[i];
            if (bfr[i] == 0) { break; }
        }
        if (i < 255) i++;
        this->outGoingMessage[i] = 0;
        this->messageInQueue = true;
        this->sendSuccess = false;
        return true;
    }
    Serial.println("Queue not available.");
    return false;
}


bool LoraRockblock::available() {
    Serial.print("LORA STATUS: ");
    Serial.print("joining "); Serial.print(this->joining);
    Serial.print(" sending "); Serial.print(this->sending);
    Serial.print(" command waiting "); Serial.print(this->commandWaiting);
    Serial.print(" enabled "); Serial.println(this->enabled);
    return (
        !(this->joining || this->sending || this->commandWaiting) &&
        this->enabled);
}


void LoraRockblock::createMessageCommand() {
    bool confirm = 1;
    uint8_t trials = 5;
    char hex_bfr[3] = {0};
    size_t msg_len = strlen(this->outGoingMessage);
    size_t cmd_len = snprintf(
        (char*) this->nextCommand, 255, "AT+DTRX=%d,%d,%d,", confirm, trials,
        msg_len);
    for (size_t i=0; i < msg_len; i++) {
        snprintf(
            this->nextCommand + cmd_len, 3, "%02X",
            int(this->outGoingMessage[i]));
        cmd_len += 2;
    }
    this->nextCommand[cmd_len] = 0x0d;
    this->sending = true;
    this->commandWaiting = true;
    this->messageInQueue = false;
}

/*
 * Read LoraModule response, used to wait for delayed or unsolicited messages.
 * We are using this in order to keep the main loop going.
 */
void LoraRockblock::readResponse(char *buffer) {
    size_t idx = 0;
    while(this->serial->available()) {
        buffer[idx] = this->serial->read();
        if (idx > 254) {
            Serial.println("Read buffer overflow!");
            // TODO: check whether that makes sense
            idx=0;
        } else idx++;
    }
    buffer[idx] = 0;
}

// getters for private properties
int32_t LoraRockblock::getRssi() {
    return this->lastRssi;
}

size_t LoraRockblock::getLastMessage(char *bfr) {
    size_t len = strlen(this->lastMessage);
    memcpy(bfr, this->lastMessage, len);
    bfr[len] = 0;
    return len;
}

bool LoraRockblock::getSendSuccess() {
    return this->sendSuccess;
}

bool LoraRockblock::getEnabled() {
    return this->enabled;
}

// parse message from buffer
size_t LoraRockblock::parseMessage(char * bfr) {
    char *s;
    char numberString[3] = {0};
    size_t len = 0;
    s = strstr(bfr, (char*) "OK+RECV:");
    if (s != NULL) {
        memcpy(numberString, s+14, 2);
        len = std::stoi(numberString, 0, 16);
        for (size_t i=0; i<len; i++) {
            memcpy(numberString, s+17+2*i, 2);
            this->lastMessage[i] = std::stoi(numberString, 0, 16);
        }
        this->lastMessage[len] = 0;
        return len;
    }
    this->lastMessage[0] = 0;
    return 0;
}


void LoraRockblock::parseResponse(char *bfr) {
    Serial.print(bfr);
    this->event = NOEVENT;
    if (matchBuffer(bfr, (char*) "CJOIN:OK")) event = JOIN_SUCCESS;
    if (matchBuffer(bfr, (char*) "CJOIN:FAIL")) event = JOIN_FAILURE;
    if (matchBuffer(bfr, (char*) "OK+RECV")) {
        parseMessage(bfr);
        event = SEND_SUCCESS;
    }
    if(matchBuffer(bfr, (char*) "ERR+SEND")) event = SEND_FAILURE;
    if(matchBuffer(bfr, (char*) "CME ERROR") && this->sending) {
        event = SEND_FAILURE;
    }
    if(matchBuffer(bfr, (char*) "receive data:")) {
        this->lastRssi = parseValue(bfr, (char*) "rssi = ", 7, ',');
        this->lastSnr = parseValue(bfr, (char*) "snr = ", 6, ',');
        this->lastDr = parseValue(bfr, (char*) "datarate = ", 11, '\r');
    }
    bfr[0] = 0;
}


/*
 * Run the Lora loop.
 *
 * There are three steps:
 *  1. Read serial
 *  2. Update state
 *  3. Take actions based on state.
 */
void LoraRockblock::loop() {
    static char responseBuffer[256] = {0};
    // Read serial
    this->readResponse(responseBuffer);
    Serial.write(responseBuffer);
    // Parse serial response and issue events
    this->parseResponse(responseBuffer);

    Serial.print("EVENT: "); Serial.println(event);
    // Update state
    switch(event) {

        case NOEVENT: {}
        break;

        case JOIN_SUCCESS: {
            snprintf(this->nextCommand, 64, "AT+CSAVE\n");
            this->commandWaiting = true;
            this->joining = false;
        }
        break;

        case JOIN_FAILURE: {
            this->joining = false;
        }
        break;

        case SEND_SUCCESS: {
            Serial.println("SEND SUCCESS");
            this->sending = false;
            this->sendSuccess = true;
        }
        break;

        case SEND_FAILURE: {
            Serial.println("SEND FAILURE");
            this->sending = false;
            this->sendSuccess = true;
        }
        break;
    }

    // Take actions
    if (this->available() && this->messageInQueue ) {
        Serial.println("CREATE MESSAGE");
        this->createMessageCommand();
    }

    // Send command
    if (this->commandWaiting) {
        this->serial->print(this->nextCommand);
        this->commandWaiting = false;
    }
}

/*
 * This is for future Rockblock use but does not have any effect on Lora.
 * TODO: Integrate with low power management
 */
void LoraRockblock::toggle(bool on) {
    if (!on) {

        snprintf(this->nextCommand, 32, "AT+CSLEEP=2\r");
        sendAndReceive(nextCommand, this->responseBuffer, (char*) "OK");
    }
    this->expander->pinMode(13, EXPANDER_OUTPUT);
    this->expander->digitalWrite(13, !on);
    this->enabled = on;
}