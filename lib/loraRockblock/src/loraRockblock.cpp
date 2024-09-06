#include <loraRockblock.h>

#define APP_EUI "FFFFFFFFFFFF0002"
#define DEV_EUI "FFFFFFFFFFFF0002"
#define APP_KEY "D44245A81B1220776D628D03095CA7E9"


/*
 * Parse a single integer into state.
 */
void parseInteger(char* bfr, char* keyword, size_t len, uint32_t& var) {
    char* c = strstr(bfr, keyword);
    if (c != NULL) var = atoi(bfr+len);
}

/*
 * Parse whether string exist in output.
 */
void parseBool(char* bfr, char* keyword, bool& var) {
    char* c = strstr(bfr, keyword);
    if (c != NULL) var = true;
}

/*
 * Initialize by passing IO expander and HardwareSerial reference.
 */
LoraRockblock::LoraRockblock(Expander &expander, HardwareSerial &serial) {
    this->expander = &expander;
    this->serial = &serial;
}

/*
 * Full AT serial roundtrip, use to set configuration values.
 */
void LoraRockblock::sendAT(char *command, char *bfr) {
    this->serial->print(command);
    Serial.println(command);
    vTaskDelay( pdMS_TO_TICKS( 50 ));
    this->readResponse(bfr);
    Serial.println(bfr);
}

/*
 * Send command and parse response. Returns true if expected is in bfr. Used
 * for configuration.
 */
bool LoraRockblock::sendAndCheckAT(char *command, char *bfr, char *expected) {
    this->sendAT(command, bfr);
    if ( strstr(bfr, expected) != NULL ) return true;
    return false;
}

/*
 * Configure the modem for TTI US915
 */
bool LoraRockblock::configure() {
    char command[64] = {0};
    char bfr[255] = {0};
    // restore defaults
    // snprintf(command, 64, "AT+CRESTORE\r");
    // if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CCLASS=0\r");
    // if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    // snprintf(command, 64, "AT+CRXP=0,8,923300000\r");
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CADR=1\r");
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CJOINMODE=0\r");
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CDEVEUI=%s\r", DEV_EUI);
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CAPPEUI=%s\r", APP_EUI);
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CAPPKEY=%s\r", APP_KEY);
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CFREQBANDMASK=0002\r");
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CULDLMODE=2\r");
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+CSAVE\r");
    vTaskDelay( pdMS_TO_TICKS(500) );
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    snprintf(command, 64, "AT+IREBOOT=1\r");
    if (!this->sendAndCheckAT(command, bfr, (char*) "OK")) return false;
    vTaskDelay( pdMS_TO_TICKS(500) );
    return true;
}


void LoraRockblock::beginJoin() {
    char command[] = "AT+CJOIN=1,1,8,16\r";
    this->serial->write(command);
}

/*
 * New simplified way to send message since we can get status from device and
 * don't need to construct.
 */
void LoraRockblock::sendMessage(char *bfr, size_t len) {
    // TODO: move to wherever we keep track of config
    bool confirm = 1;
    uint8_t trials = 5;
    // local buffer
    char command[255] = {0};
    char hex_bfr[3] = {0};
    size_t msg_len = strlen(bfr);
    size_t cmd_len = snprintf(
        (char*) command, 255, "AT+DTRX=%d,%d,%d,", confirm, trials,
        msg_len);
    for (size_t i=0; i < msg_len; i++) {
        snprintf(command + cmd_len, 3, "%02X", int(bfr[i]));
        cmd_len += 2;
    }
    command[cmd_len] = 0x0d;
    this->serial->write(command);
    this->sendSuccess = 0;
    this->sending = 1;
}


bool LoraRockblock::available() {
    if (!this->sending) {
        return true;
    }
}

/*
 * Read LoraModule response. Only used for configuration. Eliminate?
 */
void LoraRockblock::readResponse(char *buffer) {
    size_t idx = 0;
    while(this->serial->available()) {
        buffer[idx] = this->serial->read();
        if (idx > 253) {
            Serial.println("Read buffer overflow!");
            break;
        } else idx++;
    }
    buffer[idx] = 0;
}

// getters for private properties
int32_t LoraRockblock::getRssi() {
    return this->lastRssi;
}

/*
 * Getter for last message.
 */
size_t LoraRockblock::getLastMessage(char *bfr) {
    size_t len = strlen(this->lastMessage);
    memcpy(bfr, this->lastMessage, len);
    bfr[len] = 0;
    return len;
}

bool LoraRockblock::getSendSuccess() {
    return this->sendSuccess;
}

/*
 * Parse incoming message LoraWAN message.
 */
void LoraRockblock::parseMessage(char *bfr) {
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
    }
}

/*
 * Parse serial response from ASR6501 into class state. This is incomplete and
 * should be extended as needed.
 */
void LoraRockblock::parseState(char *bfr) {
    parseInteger(bfr, (char*) "+CSTATUS:", 9, this->status);
    parseBool(bfr, (char*) "+CJOIN:OK", this->joinOk);
    parseBool(bfr, (char*) "+CJOIN:FAIL", this->joinFailure);
    this->parseMessage(bfr);
}

/*
 * Read serial and invoke parser.
 */
void LoraRockblock::readSerial() {
    size_t idx = 0;
    char bfr[512] = {0};
    while(this->serial->available()) {
        bfr[idx] = this->serial->read();
        if (bfr[idx] == '\r' || bfr[idx] == '\n') {
            bfr[idx] = 0;
            this->parseState(bfr);
            idx = 0;
        } else idx++;
    }
}


/*
 * Update state and correct inconsistencies.
 */
void LoraRockblock::updateStateLogic() {
    if ( this->joinOk ) this->joinFailure = false;
    if ( this->joinFailure ) this->joinOk = false;
    if ( (this->status == 7 || this->status == 8) && this->sending ) {
        this->sendSuccess = true;
        this->sending = false;
    };
 }

/*
 * Loop to be called in task. It does three things for now:
 *  - read and parse serial
 *  - update the state logic
 *  - and send a state check command (incomplete, we could add link checks here)
 */
void LoraRockblock::loop() {
    this->readSerial();
    this->updateStateLogic();
    // Serial.print("Status: "); Serial.print(this->status);
    // Serial.print(", Join ok: "); Serial.print(this->joinOk);
    // Serial.print(", Join failure: "); Serial.print(this->joinFailure);
    // Serial.print(", send success: "); Serial.print(this->sendSuccess);
    // Serial.print(", last incoming: "); Serial.println(this->lastMessage);
    this->serial->write("AT+CSTATUS?\r");
}

/*
 * This is for future Rockblock use but does not have any effect on Lora.
 * TODO: Integrate with low power management
 */
void LoraRockblock::toggle(bool on) {
    if (!on) this->serial->println("AT+CSLEEP=2\r");
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    this->expander->pinMode(13, EXPANDER_OUTPUT);
    this->expander->digitalWrite(13, !on);
    this->enabled = on;
}