/*
 * Use this for debugging
 */
#ifndef __DISPLAY__
#define __DISPLAY__

// see https://www.hackster.io/patrick-fitzgerald2/ttgo-t-display-77905d
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_SSD1306.h> // Hardware-specific library for ST7789
#include <SPI.h>

// OLED PIN seem to be part of the board's definition
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

class LilyGoDisplay {

    private:
        Adafruit_SSD1306 displ;
        TwoWire* wire;
        char *content;

    public:
        LilyGoDisplay(TwoWire& i2c);
        void begin();
        void loop();
        void set(char *bfr);

};

#endif