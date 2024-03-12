# include <loraRockblock.h>

LoraRockblock::LoraRockblock(Expander &expander, HardwareSerial &serial) {
    this->expander = &expander;
    this->serial = &serial;
}


void LoraRockblock::enable() {
    if (this->serial->available()) this->serial->read();
    // toggle Rockblock on, no effect on Lora module
    this->expander->pinMode(13, EXPANDER_OUTPUT);
    this->expander->digitalWrite(13, true);
    this->expander->pinMode(1, EXPANDER_OUTPUT);
    this->expander->digitalWrite(1, true);
    this->expander->pinMode(2, EXPANDER_OUTPUT);
    this->expander->digitalWrite(2, false);
    this->enabled = true;
}


void LoraRockblock::disable() {
    // togge off Rockblock (no effect on Lora module)
    this->expander->pinMode(13, EXPANDER_OUTPUT);
    this->expander->digitalWrite(13, true);
    this->enabled = false;
    if (this->serial->available()) this->serial->read();
}


void LoraRockblock::loop() {
    // make sure buffer is empty before we send command
    while (this->serial->available()) this->serial->read();
    this->serial->println("AT!\n");
    vTaskDelay( pdMS_TO_TICKS(100) );
    Serial.print("Read Lora: ");
    while (this->serial->available()) {
        Serial.print((char) this->serial->read());
    }
    Serial.println();
    vTaskDelay( pdMS_TO_TICKS(250) );
}


void LoraRockblock::toggle(bool on) {
    if (on) {
        this->enable();
    } else {
        this->disable();
    }
}