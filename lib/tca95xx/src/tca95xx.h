/*
 * Class to interact with port expander mimicking Arduino port methods.
 */
#ifndef __TCA95XX_H__
#define __TCA95XX_H__
#include <Arduino.h>
#include <stdint.h>
#include <Wire.h>
#include <tuple>


#define EXPANDER_OUTPUT 0
#define EXPANDER_INPUT 1


const uint8_t TCA95_DEFAULTS[8] = {
  0b00000000,  // Input register level port 0 (read only)
  0b00000000,  // Input register level port 1 (read only)
  0b00000000,  // Output register level port 0
  0b00000000,  // Output register level port 1
  0b00000000,  // Pin polarity port 0
  0b00000000,  // Pin polarity port 1
  0b11111111,  // IO direction 1 ... input, 0 ... output port 0
  0b11111111   // IO direction 1 ... input, 0 ... output port 1
};

// This seems to be a good way to test whether we have power on the bus
// https://forum.arduino.cc/t/i2c-is-it-possible-to-catch-error-no-bus-or-no-slave/456662


class Expander {

private:
    uint8_t address;
    // Get PORT and BIT from PIN
    std::tuple<uint8_t, uint8_t> get_port_and_bit(uint8_t pin);
    // Set a single BIT on a BYTE
    uint8_t set_bit(uint8_t old_byte, uint8_t pos, bool value);
    // Modify a single BIT in a TC95xx REGISTER
    void modify(uint8_t addr, uint8_t pos, bool value);
    // Read a BYTE from the expander
    uint8_t read(uint8_t addr);
    TwoWire* wire;

public:
    Expander(TwoWire& i2c);
    // configure Expander with an I2C address
    void begin(uint8_t i2c_address);
    // Set default values
    void init();
    // overload methods to be compatible with Arduino's pinMode, digitalWrite,
    // digitalRead
    void pinMode(uint8_t pin, bool mode);
    void pinMode(uint8_t port, uint8_t bit, bool mode);
    void digitalWrite(uint8_t pin, bool value);
    void digitalWrite(uint8_t port, uint8_t bit, bool value);
    bool digitalRead(uint8_t pin);
    bool digitalRead(uint8_t port, uint8_t bit);
};

#endif /* __TCA95XX_H__ */
