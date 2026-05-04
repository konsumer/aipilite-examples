#include <Arduino.h>
#include <FastLED.h>
#include "../pins.h"

#define NUM_LEDS 1

CRGB leds[NUM_LEDS];
uint8_t hue = 0;

void setup() {
  pinMode(PIN_PWR_CTL, OUTPUT);
  digitalWrite(PIN_PWR_CTL, HIGH);

  FastLED.addLeds<WS2812, PIN_RGB_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(64); // 0-255
}

void loop() {
  hue++;
  leds[0] = CHSV(hue, 255, 255);
  FastLED.show();
  delay(20);
}
