/**
 *  Display Controller header
 * 
 **/

#ifndef _DISPLAY_CONTROLER_H
#define _DISPLAY_CONTROLLER_H


#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RST 16 // GPIO 0 OLED Reset


/**
 * Display Controller - Deals with drawing to the OLED display if enabled
 *
 **/

 class DisplayController {

     private:
     
        Adafruit_SSD1306 display = Adafruit_SSD1306(OLED_RST);

     public:

        void drawHeader(String title);
        void drawNumber(int x, int y, int Number, String text);
        void drawHead(int x, int y);
        void drawBooting();
        void redrawDisplay(int, int, int , bool);
        void setupDisplay();

 };

#endif