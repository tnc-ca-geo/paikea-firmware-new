#include "rockblock.h"
#include <ArduinoFake.h>
#include <SerialFake.h>

using namespace fakeit;

Serial_ rockblock_serial;
Expander expander = Expander(Wire);

void test_test() {
    Rockblock rockblock = Rockblock(expander, rockblock_serial);
};