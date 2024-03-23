#include "tca95xx.h"

/*
 * Constructure passing a wire TwoWire instance.
 */
Expander::Expander(TwoWire& i2c) {
    wire = &i2c;
}

/*
 * Initialize the Expander
 */
void Expander::begin(uint8_t i2c_address) {
    this->address = i2c_address;
    this->init();
}

/*
 * PRIVATE: Get PORT and BIT from PIN
 *
 * For use of tuple see
 * https://stackoverflow.com/questions/321068/returning-multiple-values-from-a-c-function
 */
std::tuple<uint8_t, uint8_t> Expander::get_port_and_bit(uint8_t pin) {
    uint8_t port = 0;
    uint8_t bit = pin;
    if (pin > 7) return {1, pin-10};
    return {0, pin};
}

/*
 * PRIVATE: Set a single BIT of a BYTE
 *
 * TODO: there is probably a fancier way to do that. Not relying on Arduino
 * predefined functions.
 */
uint8_t Expander::set_bit(uint8_t old_byte, uint8_t pos, bool value) {
    if (value) return bitSet(old_byte, pos);
    return bitClear(old_byte, pos);
};

/*
 * PRIVATE: Read a BYTE from the expander
 */
uint8_t Expander::read(uint8_t addr) {
    wire->beginTransmission(this->address);
    wire->write(addr);
    wire->endTransmission();
    wire->requestFrom(this->address, 1);
    uint8_t value = wire->read();
    wire->endTransmission();
    return value;
};

/*
 * PRIVATE: Modify a single BIT in a TC95xx REGISTER
 */
void Expander::modify(uint8_t addr, uint8_t pos, bool value) {
    uint8_t current_value = this->read(addr);
    uint8_t new_value = set_bit(current_value, pos, value);
    wire->beginTransmission(this->address);
    wire->write(addr);
    wire->write(new_value);
    wire->endTransmission();
};

/*
 * Set default values
 */
void Expander::init() {
  for (uint8_t i=0; i<8; i++) {
    wire->beginTransmission(this->address);
    wire->write(i);
    wire->write(TCA95_DEFAULTS[i]);
    wire->endTransmission();
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
