// Battery monitor demo for AIPI-Lite.
// GPIO2 is battery ADC: ADC1 CH1, 12dB attenuation, ×2.0 voltage divider.
// LiPo range: ~3.0V (empty) to 4.2V (full).

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../pins.h"

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7735(bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);

static const float BAT_MIN = 3.0f;
static const float BAT_MAX = 4.2f;

// Returns battery voltage in volts (×2.0 scale for the voltage divider).
float readBatteryVolts() {
    return analogReadMilliVolts(PIN_BAT_ADC) * 2.0f / 1000.0f;
}

// Clamps percent to 0–100.
int voltageToPercent(float v) {
    if (v <= BAT_MIN) return 0;
    if (v >= BAT_MAX) return 100;
    return (int)((v - BAT_MIN) / (BAT_MAX - BAT_MIN) * 100.0f);
}

void drawBattery(float volts, int pct) {
    uint16_t barColor = (pct > 60) ? RGB565_GREEN
                      : (pct > 25) ? RGB565_ORANGE
                      :              RGB565_RED;

    // Voltage
    gfx->fillRect(0, 30, 128, 30, RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(10, 35);
    gfx->printf("%.2fV", volts);

    // Percent text
    gfx->fillRect(0, 62, 128, 20, RGB565_BLACK);
    gfx->setTextColor(barColor);
    gfx->setCursor(10, 65);
    gfx->printf("%d%%", pct);

    // Bar outline
    gfx->drawRect(10, 88, 100, 18, RGB565_WHITE);
    // Nub on right side of the battery icon
    gfx->fillRect(110, 93, 5, 8, RGB565_WHITE);

    // Bar fill
    int fillW = (int)(98.0f * pct / 100.0f);
    gfx->fillRect(11, 89, 98, 16, RGB565_BLACK);
    if (fillW > 0) {
        gfx->fillRect(11, 89, fillW, 16, barColor);
    }
}

void setup() {
    pinMode(PIN_PWR_CTL, OUTPUT);
    digitalWrite(PIN_PWR_CTL, HIGH);

    Serial.begin(115200);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);

    gfx->begin();
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(4, 4);
    gfx->println("AIPI-Lite Battery");
    gfx->drawFastHLine(0, 16, 128, RGB565_WHITE);

    // 12dB attenuation gives full 0–3.3V ADC input range
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
}

void loop() {
    float v = readBatteryVolts();
    int pct = voltageToPercent(v);

    drawBattery(v, pct);
    Serial.printf("Battery: %.2fV  %d%%\n", v, pct);

    delay(2000);
}
