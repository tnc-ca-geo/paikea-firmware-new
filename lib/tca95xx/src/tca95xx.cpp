#include "tca95xx.h"


Expander::Expander() {};

// for use of tuple see here
// https://stackoverflow.com/questions/321068/returning-multiple-values-from-a-c-function
std::tuple<uint8_t, uint8_t> Expander::get_port_and_bit(uint8_t pin) {
    uint8_t port = 0;
    uint8_t bit = pin;
    if (pin > 7) return {1, pin-10};
    return {0, pin};
}

// Modify a single bit of a byte, there is probably a fancier way to do that.
// Not relying on Arduino predefined functions.
uint8_t Expander::set_bit(uint8_t old_byte, uint8_t pos, bool value) {
    if (value) return bitSet(old_byte, pos);
    return bitClear(old_byte, pos);
};

uint8_t Expander::read(uint8_t addr) {
    Wire.beginTransmission(this->address);
    Wire.write(addr);
    // This seems to be necessary.
    Wire.endTransmission();
    Wire.requestFrom(this->address, 1);
    uint8_t value = Wire.read();
    Wire.endTransmission();
    return value;
};

// Update a register
void Expander::modify(uint8_t addr, uint8_t pos, bool value) {
    uint8_t current_value = this->read(addr);
    uint8_t new_value = set_bit(current_value, pos, value);
    Wire.beginTransmission(this->address);
    Wire.write(addr);
    Wire.write(new_value);
    Wire.endTransmission();
};

void Expander::begin(uint8_t i2_address) {
    this->address = i2_address;
    Wire.begin();
    this->init();
}

// Configure default values
void Expander::init() {

  for (uint8_t i=0; i<8; i++) {
    Wire.beginTransmission(this->address);
    Wire.write(i);
    Wire.write(TCA95_DEFAULTS[i]);
    Wire.endTransmission();
  }
}

void Expander::pinMode(uint8_t pin, bool mode) {
    using namespace std;
    uint8_t port, bit;
    tie(port, bit) = this->get_port_and_bit(pin);
    this->pinMode(port, bit, mode);
};

void Expander::pinMode(uint8_t port, uint8_t pos, bool mode) {
    const uint8_t reg = 6 + port;
    this->modify(reg, pos, mode);
};

void Expander::digitalWrite(uint8_t pin, bool value) {
    using namespace std;
    uint8_t port, pos;
    tie(port, pos) = this->get_port_and_bit(pin);
    this->digitalWrite(port, pos, value);
}

void Expander::digitalWrite(uint8_t port, uint8_t pos, bool value) {
    const uint8_t reg = 2 + port;
    this->modify(reg, pos, value);
};

bool Expander::digitalRead(uint8_t pin) {
    using namespace std;
    uint8_t port, pos;
    tie(port, pos) = this->get_port_and_bit(pin);
    return this->digitalRead(port, pos);
}

bool Expander::digitalRead(uint8_t port, uint8_t pos) {
    return this->read(port) & 1 << pos;
}
