// RGB LED demo - WS2812 on GPIO 46
#include <Arduino.h>
#include <FastLED.h>
#include "../pins.h"

#define NUM_LEDS 1        // Single LED       // WS2812 data pin (GPIO 46)

CRGB leds[NUM_LEDS];
uint8_t hue = 0;

void setup() {
  pinMode(PIN_PWR_CTL, OUTPUT);
  digitalWrite(PIN_PWR_CTL, HIGH);

  Serial.begin(115200);

  // Initialize FastLED with WS2812 (NeoPixel) on GPIO 46
  FastLED.addLeds<WS2812, PIN_RGB_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(64); // 0-255

  Serial.println("RGB LED demo started - WS2812 on GPIO 46");
}

void loop() {
  // Rainbow cycle
  hue++;
  leds[0] = CHSV(hue, 255, 255);
  FastLED.show();
  delay(20);
}
