// Basic demo for AIPI-Lite — display + buttons
// ST7735 128x128, SPI: CLK=16 MOSI=17 DC=7 RST=18 CS=15 BL=3
// Buttons: A=GPIO1, B=GPIO42 (active LOW)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../pins.h"


Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7735(bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);

struct Button {
  int pin, state;
  unsigned long lastChange;
};

static Button btns[] = {{PIN_BTN_A, HIGH, 0}, {PIN_BTN_B, HIGH, 0}};

bool readButton(Button &btn) {
  int raw = digitalRead(btn.pin);
  if (raw == btn.state) { btn.lastChange = 0; return false; }
  if (!btn.lastChange) { btn.lastChange = millis(); return false; }
  if (millis() - btn.lastChange < 50) return false;
  btn.state = raw;
  btn.lastChange = 0;
  return true;
}

void drawButtons() {
  int y = gfx->height() / 2 - 10;
  gfx->fillRect(0, y, gfx->width(), 30, RGB565_BLACK);
  gfx->setCursor(10, y + 5);
  gfx->setTextSize(2);
  gfx->setTextColor(btns[0].state == LOW ? RGB565_RED : RGB565_WHITE);
  gfx->print("A");
  gfx->setTextColor(btns[1].state == LOW ? RGB565_RED : RGB565_WHITE);
  gfx->print("  B");
}

void setup() {
  pinMode(PIN_PWR_CTL, OUTPUT);
  digitalWrite(PIN_PWR_CTL, HIGH);

  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  Serial.begin(115200);

  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 20);
  gfx->println("AIPI-Lite");

  drawButtons();
}

void loop() {
  bool changed = false;
  for (auto &btn : btns) changed |= readButton(btn);
  if (changed) {
    drawButtons();
    Serial.printf("A:%s B:%s\n",
      btns[0].state == LOW ? "DOWN" : "up",
      btns[1].state == LOW ? "DOWN" : "up");
  }
  delay(5);
}
