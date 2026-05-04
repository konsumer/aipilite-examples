// AIPI-Lite looper: hold A to record into PSRAM, release to stop; press B to play back.
// ES8311 codec via I2C, ESP32-S3 as I2S master, 16-bit stereo 24 kHz.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "../pins.h"

// Display
Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7735(bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);

// Audio constants
static const int    SAMPLE_RATE  = 24000;
static const int    CHANNELS     = 2;
static const int    BITS         = 16;
static const size_t FRAME_BYTES  = CHANNELS * (BITS / 8);
static const size_t MAX_SECONDS  = 30;
static const size_t BUF_BYTES    = SAMPLE_RATE * FRAME_BYTES * MAX_SECONDS;

// PSRAM recording buffer
static uint8_t *recBuf  = nullptr;
static size_t   recLen  = 0;
static size_t   playPos = 0;

// Audio objects — board must be declared before i2s so it's alive at i2s construction
AudioInfo audioInfo(SAMPLE_RATE, CHANNELS, BITS);
DriverPins myPins;
AudioBoard board(AudioDriverES8311, myPins);
I2SCodecStream i2s(board);

enum State { IDLE, RECORDING, PLAYING };
static State state = IDLE;

static uint8_t chunk[512];

void showStatus(const char *msg, uint16_t color = RGB565_WHITE) {
    gfx->fillRect(0, 35, 128, 60, RGB565_BLACK);
    gfx->setTextColor(color);
    gfx->setTextSize(2);
    gfx->setCursor(4, 50);
    gfx->println(msg);
}

void setup() {
    pinMode(PIN_PWR_CTL, OUTPUT);
    digitalWrite(PIN_PWR_CTL, HIGH);

    Serial.begin(115200);

    // Display
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    gfx->begin();
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(4, 4);
    gfx->println("AIPI Looper");
    gfx->setCursor(4, 14);
    gfx->println("A:Rec  B:Play");

    // Buttons
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);

    // Speaker amp
    pinMode(PIN_SPKR_EN, OUTPUT);
    digitalWrite(PIN_SPKR_EN, HIGH);

    // Allocate PSRAM recording buffer (~30s of audio)
    recBuf = (uint8_t *)ps_malloc(BUF_BYTES);
    if (!recBuf) {
        showStatus("PSRAM FAIL", RGB565_RED);
        while (true) delay(1000);
    }

    // Configure codec I2C and I2S pins — I2SCodecStream picks these up automatically
    myPins.addI2C(PinFunction::CODEC, I2C_SCL, I2C_SDA);
    myPins.addI2S(PinFunction::CODEC, I2S_MCLK, I2S_BCLK, I2S_LRCLK, I2S_DOUT, I2S_DIN);

    // Init codec + I2S together; is_master=true tells library codec should be slave
    auto cfg = i2s.defaultConfig(RXTX_MODE);
    cfg.copyFrom(audioInfo);
    cfg.input_device  = ADC_INPUT_LINE1;
    cfg.output_device = DAC_OUTPUT_ALL;
    cfg.is_master     = true;
    cfg.mclk_multiple = 256;  // MCLK = 256*fs = 6.144 MHz per hardware spec
    i2s.begin(cfg);

    i2s.setVolume(0.8f);
    i2s.setInputVolume(0.8f);

    showStatus("READY");
}

void loop() {
    bool btnA = (digitalRead(PIN_BTN_A) == LOW);
    bool btnB = (digitalRead(PIN_BTN_B) == LOW);

    switch (state) {
        case IDLE:
            if (btnA) {
                recLen = 0;
                state = RECORDING;
                showStatus("REC...", RGB565_RED);
            } else if (btnB && recLen > 0) {
                playPos = 0;
                state = PLAYING;
                showStatus("PLAY", RGB565_GREEN);
            }
            break;

        case RECORDING:
            if (!btnA) {
                state = IDLE;
                char buf[20];
                float secs = (float)recLen / FRAME_BYTES / SAMPLE_RATE;
                snprintf(buf, sizeof(buf), "%.1fs saved", secs);
                showStatus(buf);
            } else if (recLen < BUF_BYTES) {
                size_t n = i2s.readBytes(recBuf + recLen,
                                         min(sizeof(chunk), BUF_BYTES - recLen));
                recLen += n;
                if (recLen >= BUF_BYTES) {
                    state = IDLE;
                    showStatus("BUF FULL");
                }
            }
            break;

        case PLAYING:
            if (btnA || playPos >= recLen) {
                state = IDLE;
                showStatus("READY");
            } else {
                size_t n = min(sizeof(chunk), recLen - playPos);
                i2s.write(recBuf + playPos, n);
                playPos += n;
            }
            break;
    }
}
