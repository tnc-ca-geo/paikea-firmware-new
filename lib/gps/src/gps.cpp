#include <gps.h>


const uint8_t BUFFER_SIZE = 255;


Gps::Gps(Expander &expander, HardwareSerial &serial) {
    this->expander = &expander;
    this->serial = &serial;
}


void Gps::enable() {
    // flush Serial
    if (this->serial->available()) this->serial->read();
    this->expander->pinMode(0, EXPANDER_OUTPUT);
    this->expander->digitalWrite(0, HIGH);
    this->enabled = true;
}


void Gps::disable() {
    this->enabled = false;
    this->expander->digitalWrite(0, LOW);
    // flush Serial
    if (this->serial->available()) this->serial->read();
}


void Gps::loop() {
    char character;
    while(this->serial->available()) {
        character = this->serial->read();
        this->gps_parser.encode(character);
    }
    if (
        this->gps_parser.time.isValid() && gps_parser.time.isUpdated() &&
        this->gps_parser.date.isValid() && gps_parser.date.isUpdated() &&
        this->gps_parser.location.isValid() && gps_parser.location.isUpdated()
    ) {
        this->gps_read_system_time = esp_timer_get_time();
        this->epoch = this->time_to_epoch(
            this->gps_parser.date, this->gps_parser.time);
        this->lat = this->gps_parser.location.lat();
        this->lng = this->gps_parser.location.lng();
        this->updated = true;
    } else this->updated = false;
}

uint64_t Gps::get_corrected_epoch() {
    return this->epoch + (
        esp_timer_get_time() - this->gps_read_system_time )/1E6;
};

/*
 * Read one line from the GPS and store into buffer. This is currently unused
 * because the TinyGPSPlus library is managing this.
 */
void Gps::readln(char *buffer) {
    uint8_t i=0;
    buffer[0] = 0;
    while (this->serial->available()) {
        uint8_t c=this->serial->read();
        if ((c == '\n') || (c == '\r') || (i > BUFFER_SIZE)) {
            buffer[i] = 0;
            break;
        }
        buffer[i] = c;
        i++;
    }
}


// assuming that 1 second precision is enough for our purposes
time_t Gps::time_to_epoch(TinyGPSDate date, TinyGPSTime time) {
    struct tm t={0};
    t.tm_year = date.year() - 1900;
    t.tm_mon = date.month() - 1;
    t.tm_mday = date.day();
    t.tm_hour = time.hour();
    t.tm_min = time.minute();
    t.tm_sec = time.second();
    return mktime(&t);
}