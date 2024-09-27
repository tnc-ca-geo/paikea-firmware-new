#include "display.h"

LilyGoDisplay::LilyGoDisplay(TwoWire& i2c) {
  displ = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &i2c, OLED_RST);
}

void LilyGoDisplay::begin() {
  if(!this->displ.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    // for(;;); // Don't proceed, loop forever
  }
}

void LilyGoDisplay::set(char *bfr) {
  uint8_t i=0;
  this->displ.clearDisplay();
  this->displ.setTextColor(WHITE);
  this->displ.setTextSize(2);
  this->displ.setCursor(0,0);
  while(bfr[i] > 0) {
    this->displ.print(bfr[i]);
    i++;
  }
  this->displ.display();
}

// put the display in sleep state
void LilyGoDisplay::off() {
  this->displ.clearDisplay();
  this->displ.display();
}

void LilyGoDisplay::loop() {
}