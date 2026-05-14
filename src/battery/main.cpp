// Battery monitor demo for AIPI-Lite.
// GPIO2 is battery ADC: ADC1 CH1, 12dB attenuation, ×2.0 voltage divider.
// LiPo range: ~3.0V (empty) to 4.2V (full).
// Sleeps after SLEEP_TIMEOUT_MS of inactivity; press A to wake.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "../pins.h"

#define SLEEP_TIMEOUT_MS 10000

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7735(bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);

static const float BAT_MIN = 3.0f;
static const float BAT_MAX = 4.2f;
static unsigned long lastActivity;

float readBatteryVolts() {
    return analogReadMilliVolts(PIN_BAT_ADC) * 2.0f / 1000.0f;
}

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
    // Nub on right side of battery icon
    gfx->fillRect(110, 93, 5, 8, RGB565_WHITE);

    // Bar fill
    int fillW = (int)(98.0f * pct / 100.0f);
    gfx->fillRect(11, 89, 98, 16, RGB565_BLACK);
    if (fillW > 0) {
        gfx->fillRect(11, 89, fillW, 16, barColor);
    }
}

void drawBg() {
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(4, 4);
    gfx->println("AIPI-Lite Battery");
    gfx->drawFastHLine(0, 16, 128, RGB565_WHITE);
}

void goToSleep() {
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(20, 55);
    gfx->println("Sleeping...");
    gfx->setCursor(10, 70);
    gfx->println("Press A to wake");

    // delay a second, but allow button to cancel
    int t = millis();
    while (millis() - t < 1000) {
        if (digitalRead(PIN_BTN_A) == LOW) {
            lastActivity = millis();
            drawBg();
            return;
        }
    }

    digitalWrite(PIN_LCD_BL, LOW);

    // Hold PWR_CTL high so the power circuit stays latched during deep sleep
    rtc_gpio_hold_en((gpio_num_t)PIN_PWR_CTL);

    // Wake when Button A is pressed (active LOW)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_A, 0);

    esp_deep_sleep_start();
}

void setup() {
    // Release any GPIO hold left from before deep sleep (harmless on first boot)
    rtc_gpio_hold_dis((gpio_num_t)PIN_PWR_CTL);

    pinMode(PIN_PWR_CTL, OUTPUT);
    digitalWrite(PIN_PWR_CTL, HIGH);

    Serial.begin(115200);

    pinMode(PIN_BTN_A, INPUT_PULLUP);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);

    gfx->begin();
    drawBg();

    // 12dB attenuation gives full 0–3.3V ADC input range
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

    lastActivity = millis();
}

void loop() {
    if (digitalRead(PIN_BTN_A) == LOW) {
        lastActivity = millis();
    }

    if (millis() - lastActivity >= SLEEP_TIMEOUT_MS) {
        goToSleep();
    }

    float v = readBatteryVolts();
    int pct = voltageToPercent(v);

    drawBattery(v, pct);
    Serial.printf("Battery: %.2fV  %d%%\n", v, pct);

    delay(2000);
}
