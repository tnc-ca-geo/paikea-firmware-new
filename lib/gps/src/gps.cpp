#include <gps.h>

const uint8_t BUFFER_SIZE = 255;

HardwareSerial gps_serial(2);
TinyGPSPlus gps_parser;

Gps::Gps() {}

void Gps::begin(uint8_t rx, uint8_t tx) {
    gps_serial.begin(9600, SERIAL_8N1, rx, tx);
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

void Gps::loop() {
    // this->readln(this->read_buffer);
    while(gps_serial.available()) { gps_parser.encode(gps_serial.read()); }
    if (gps_parser.date.isValid() && gps_parser.time.isValid()) {
        this->gps_read_system_time = esp_timer_get_time();
        this->epoch = this->time_to_epoch(gps_parser.date, gps_parser.time);
        this->valid = gps_parser.date.isUpdated();
    } else {
        this->valid = false;
    }
}

/*
 * Read one line from the GPS and store into buffer. This is currently unused
 * because the TinyGPSPlus library is managing this.
 */
void Gps::readln(char *buffer) {
    uint8_t i=0;
    buffer[0] = 0;
    while (gps_serial.available()) {
        uint8_t c=gps_serial.read();
        if ((c == '\n') || (c == '\r') || (i > BUFFER_SIZE)) {
            buffer[i] = 0;
            break;
        }
        buffer[i] = c;
        i++;
    }
}