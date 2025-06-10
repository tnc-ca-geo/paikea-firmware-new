#include <gps.h>

Gps::Gps(Expander &expander, HardwareSerial &serial, uint8_t enable_pin) {
    this->expander = &expander;
    this->serial = &serial;
    this->enable_pin = enable_pin;
}

/*
 * Turn GPS on via port expander
 */
void Gps::enable() {
    // flush Serial buffer
    if (this->serial->available()) this->serial->read();
    this->expander->pinMode(this->enable_pin, EXPANDER_OUTPUT);
    this->expander->digitalWrite(this->enable_pin, HIGH);
    this->enabled = true;
    this->start_time = esp_timer_get_time() / 1E6;
}

/*
 * Turn GPS off via port expander
 */
void Gps::disable() {
    this->enabled = false;
    this->expander->digitalWrite(this->enable_pin, LOW);
}

/*
 * Run t
 */
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
        this->speed = this->gps_parser.speed.knots();
        this->heading = this->gps_parser.course.deg();
        this->updated = true;
    } else this->updated = false;
}

/*
 * This should give the actual time since it keeps track of the system time
 * and the time GPS was read.
 */
time_t Gps::get_corrected_epoch() {
    return this->epoch + (
        esp_timer_get_time() - this->gps_read_system_time ) / 1E6;
};

// assume that 1 second precision is enough for our purposes
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
