// AIPI-Lite looper: hold A to record into PSRAM, press B to play back.
// ES8311 codec via I2C; ESP32-S3 is I2S master (MCLK = 256*fs = 6.144 MHz).

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../pins.h"

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7735(bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);


void setup() {
    pinMode(PIN_PWR_CTL, OUTPUT);
    digitalWrite(PIN_PWR_CTL, HIGH);

    Serial.begin(115200);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);

    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);

    gfx->begin();
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(10, 20);
    gfx->println("AIPI-Lite");
}

void loop() {
    delay(5);
}
