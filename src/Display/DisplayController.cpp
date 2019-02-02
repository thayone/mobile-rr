#include "DisplayController.h"



void DisplayController::drawHeader(String title) {
  display.fillRoundRect(0,0,64,11,0,WHITE);

  for (size_t i = 0; i < 128; i = i+2) {
    display.drawPixel(i,12, WHITE);
  }

  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(2,2);
  display.setTextWrap(false);
  display.println(title);
}


void DisplayController::drawNumber(int x, int y, int number, String text) {
  int number_len = (int)log10(number)+1;
  int radius = 2;
  display.fillRoundRect(x, y, 6*number_len+radius*2, 7+radius*2, radius, WHITE);

  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(x+radius,y+radius);
  display.setTextWrap(false);
  display.println(number);

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(x + 5+6*number_len+radius*2,y+radius);
  display.setTextWrap(false);
  display.println(text);
}

void DisplayController::drawHead(int x, int y) {
  // Horizontal top and bottom
  display.drawLine(x+1, y, x+5, y, WHITE);
  display.drawLine(x+1, y+4, x+5, y+4, WHITE);

  // Vertical left and right
  display.drawLine(x, y+1, x, y+3, WHITE);
  display.drawLine(x+6, y+1, x+6, y+3, WHITE);

  // Eyes left and right
  display.drawPixel(x+2,y+2, WHITE);
  display.drawPixel(x+4,y+2, WHITE);
}

void DisplayController::drawBooting() {
  drawHead((2)*8,42);
  drawHead((7)*8,42);

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,10);
  display.setTextWrap(false);
  display.println("BOOTING...");
}

void DisplayController::redrawDisplay(int rrtotal, int rrsession, int clients, bool READY) {
  display.clearDisplay();

  if(READY) {
      drawHeader("Rickrolls");
      drawNumber(0,15,rrtotal,"total");
      drawNumber(64,15,rrsession,"session");
    

    // Draw number of heads based on the number of clients connected
    int _clients = clients;
    if (_clients>8) {
      _clients = 8;
    }

    // Draw two rows of heads (4 on each row)
   int j = 0;
    for (size_t i = 1; i <= _clients; i++) {

        if (i <= 4) {
            drawHead((i-1)*8 + 1 + 70, 0);
        } else {
            drawHead(j*8 + 1 + 70, 6);
            j++;
        }
    }

  } else {
    drawBooting();
  }

  display.display();
}

void DisplayController::setupDisplay() {
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  // init done

  display.display();
  redrawDisplay(0, 0, 0, false);
}
