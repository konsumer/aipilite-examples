// AIPI-Lite looper: hold A to record into PSRAM, press B to play back.
// ES8311 codec via I2C; ESP32-S3 is I2S master (MCLK = 256*fs = 6.144 MHz).

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include "driver/i2s_std.h"
#include "esp_heap_caps.h"
#include "../pins.h"

#define SAMPLE_RATE  24000
#define MAX_SECS     30
// 16-bit stereo: 2 bytes * 2 channels * samples
static const size_t AUDIO_BUF_BYTES = (size_t)SAMPLE_RATE * 2 * 2 * MAX_SECS;

enum State { IDLE, RECORDING, PLAYING };
static State state = IDLE;
static int16_t *audioBuf = nullptr;
static size_t recBytes = 0;
static size_t playPos  = 0;

static i2s_chan_handle_t tx_chan = NULL;
static i2s_chan_handle_t rx_chan = NULL;

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7735(bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);

struct Button { int pin, state; unsigned long lastChange; };
static Button btns[] = {{PIN_BTN_A, HIGH, 0}, {PIN_BTN_B, HIGH, 0}};

// Returns true on a debounced state change; btn.state holds the new level.
static bool readButton(Button &btn) {
    int raw = digitalRead(btn.pin);
    if (raw == btn.state) { btn.lastChange = 0; return false; }
    if (!btn.lastChange)  { btn.lastChange = millis(); return false; }
    if (millis() - btn.lastChange < 50) return false;
    btn.state = raw;
    btn.lastChange = 0;
    return true;
}

// ── ES8311 ────────────────────────────────────────────────────────────────────

static void es_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t es_read(uint8_t reg) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)ES8311_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

static void initES8311() {
    Wire.begin(I2C_SDA, I2C_SCL, 100000);  // 100 kHz — more tolerant of long traces

    // ── I2C bus scan ───────────────────────────────────────────────────────
    Serial.println("I2C scan:");
    bool found = false;
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X%s\n", a, a == ES8311_ADDR ? " ← ES8311" : "");
            if (a == ES8311_ADDR) found = true;
        }
    }
    if (!found) {
        Serial.println("  ES8311 NOT found — all writes will be no-ops");
    }

    // ── Chip ID ────────────────────────────────────────────────────────────
    uint8_t id = es_read(0xFD);
    Serial.printf("ES8311 chip ID: 0x%02X (expect 0x83)\n", id);

    // ── Write/readback sanity check ────────────────────────────────────────
    es_write(0x37, 0xAA);
    uint8_t rb = es_read(0x37);
    Serial.printf("I2C write test: wrote 0xAA to REG37, read back 0x%02X (%s)\n",
                  rb, rb == 0xAA ? "OK" : "FAIL — chip not accepting writes");

    // ── Reset ──────────────────────────────────────────────────────────────
    es_write(0x00, 0x1F);
    delay(20);
    es_write(0x00, 0x00);

    // ── Clock ──────────────────────────────────────────────────────────────
    // REG01 = 0x30 is the chip's power-on-reset value.  It selects MCLK from
    // the external pad (GPIO6) with an 8× pre-divider:
    //   effective sysclk = 6.144 MHz / 8 = 768 kHz
    // That matches BCLK (24000 * 16 * 2 = 768 kHz), so LRCK_DIV must be 32.
    // 12-bit divider 32 = 0x020: REG03[7:0]=DIV[11:4]=0x02, REG04=0x00.
    es_write(0x01, 0x30);
    es_write(0x02, 0x00);
    es_write(0x03, 0x02);  // ADC LRCK div → 32
    es_write(0x04, 0x00);
    es_write(0x05, 0x02);  // DAC LRCK div → 32
    es_write(0x06, 0x00);
    es_write(0x07, 0x00);
    es_write(0x08, 0xFF);

    // ── Serial data port: I2S standard, 16-bit ────────────────────────────
    es_write(0x09, 0x0C);
    es_write(0x0A, 0x0C);

    // ── Power ──────────────────────────────────────────────────────────────
    es_write(0x0D, 0x01);
    es_write(0x0E, 0x02);  // VMID fast-charge
    delay(30);
    es_write(0x0E, 0x00);
    es_write(0x0F, 0x44);
    es_write(0x10, 0x28);
    es_write(0x11, 0x00);
    es_write(0x13, 0x10);

    // ── ADC (mic) ──────────────────────────────────────────────────────────
    es_write(0x14, 0x1A);
    es_write(0x15, 0x00);
    es_write(0x16, 0x00);
    es_write(0x17, 0xBF);
    es_write(0x18, 0x08);
    es_write(0x19, 0x00);
    es_write(0x1A, 0x00);
    es_write(0x1B, 0x00);
    // REG1C bit[6] = ADC_MUTE; 0x6A had it set → 0x2A clears it, keeps HPF on.
    es_write(0x1C, 0x2A);
    es_write(0x1F, 0xBF);

    // ── DAC (speaker) ──────────────────────────────────────────────────────
    // REG31 bit[6] = DAC_MUTE; 0x60 had it set → silenced the startup beep too.
    es_write(0x31, 0x00);
    es_write(0x32, 0x00);
    es_write(0x37, 0xBF);
    es_write(0x38, 0x00);
    es_write(0x39, 0x04);
    es_write(0x3A, 0x00);
    es_write(0x44, 0x08);

    Serial.printf("REG01=0x%02X REG03=0x%02X REG1C=0x%02X REG31=0x%02X REG37=0x%02X\n",
                  es_read(0x01), es_read(0x03), es_read(0x1C),
                  es_read(0x31), es_read(0x37));
    delay(50);
}

// ── I2S ──────────────────────────────────────────────────────────────────────

static void initI2S() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan);

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src        = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple  = I2S_MCLK_MULTIPLE_256,  // 256 × 24 kHz = 6.144 MHz
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)I2S_MCLK,
            .bclk = (gpio_num_t)I2S_BCLK,
            .ws   = (gpio_num_t)I2S_LRCLK,
            .dout = (gpio_num_t)I2S_DOUT,
            .din  = (gpio_num_t)I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    i2s_channel_init_std_mode(tx_chan, &std_cfg);
    i2s_channel_init_std_mode(rx_chan, &std_cfg);
    i2s_channel_enable(tx_chan);
    i2s_channel_enable(rx_chan);
}

// ── Diagnostics ──────────────────────────────────────────────────────────────

// 800 Hz square wave for 300 ms — audible if DAC+speaker path works.
static void playTestBeep() {
    const int TOTAL = SAMPLE_RATE * 3 / 10;  // 300 ms worth of frames
    const int HALF  = SAMPLE_RATE / 1600;    // samples per half-period of 800 Hz
    int16_t buf[128];
    size_t written;
    for (int done = 0; done < TOTAL; ) {
        int n = (TOTAL - done) < 64 ? (TOTAL - done) : 64;
        for (int i = 0; i < n; i++) {
            int16_t s = ((done + i) % (HALF * 2) < HALF) ? 20000 : -20000;
            buf[i * 2]     = s;
            buf[i * 2 + 1] = s;
        }
        i2s_channel_write(tx_chan, buf, n * 4, &written, pdMS_TO_TICKS(200));
        done += n;
    }
}

// Print peak amplitude of the recording so we can tell whether the mic captured anything.
static void logSampleStats() {
    if (recBytes < 4) return;
    int16_t peak = 0;
    size_t count = recBytes / 2;
    for (size_t i = 0; i < count; i++) {
        int16_t v = audioBuf[i] < 0 ? -audioBuf[i] : audioBuf[i];
        if (v > peak) peak = v;
    }
    Serial.printf("Sample peak: %d  (0 = silence, 32767 = full scale)\n", peak);
}

// ── Display helpers ───────────────────────────────────────────────────────────

static void drawStatus(const char *line1, const char *line2 = "") {
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(5, 8);
    gfx->println("AIPI Looper");
    gfx->drawFastHLine(0, 30, 128, RGB565_WHITE);

    gfx->setTextSize(1);
    gfx->setCursor(5, 40);
    gfx->println(line1);
    if (line2[0]) {
        gfx->setCursor(5, 54);
        gfx->println(line2);
    }

    gfx->setCursor(5, 108);
    gfx->setTextColor(RGB565_DARKGREY);
    gfx->println("A: hold=record");
    gfx->setCursor(5, 120);
    gfx->println("B: play");
}

// ── Arduino ───────────────────────────────────────────────────────────────────

void setup() {
    pinMode(PIN_PWR_CTL, OUTPUT);
    digitalWrite(PIN_PWR_CTL, HIGH);

    Serial.begin(115200);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    gfx->begin();
    drawStatus("Initializing...");

    pinMode(PIN_SPKR_EN, OUTPUT);
    digitalWrite(PIN_SPKR_EN, HIGH);

    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);

    audioBuf = (int16_t *)ps_malloc(AUDIO_BUF_BYTES);
    if (!audioBuf) {
        gfx->fillScreen(RGB565_RED);
        gfx->setCursor(5, 5);
        gfx->setTextColor(RGB565_WHITE);
        gfx->println("PSRAM alloc failed");
        Serial.println("PSRAM alloc failed");
        while (1) delay(100);
    }

    initES8311();
    initI2S();

    // Startup beep — audible = DAC+speaker path is alive.
    playTestBeep();

    drawStatus("Ready", "No recording yet");
    Serial.println("Looper ready");
}

void loop() {
    bool chA = readButton(btns[0]);
    bool chB = readButton(btns[1]);
    bool heldA  = (btns[0].state == LOW);
    bool pressB = chB && (btns[1].state == LOW);

    switch (state) {
        case IDLE:
            if (heldA && chA) {
                // A just pressed — start recording
                recBytes = 0;
                state = RECORDING;
                drawStatus("Recording...", "Release A to stop");
                Serial.println("Recording started");
            } else if (pressB) {
                if (recBytes == 0) {
                    drawStatus("Nothing recorded", "Hold A first");
                    break;
                }
                state = PLAYING;
                playPos = 0;
                drawStatus("Playing...");
                Serial.printf("Playback: %u bytes\n", recBytes);
            }
            break;

        case RECORDING: {
            if (!heldA && chA) {
                // A just released — stop recording
                state = IDLE;
                float secs = (float)recBytes / (SAMPLE_RATE * 2 * 2);
                char buf[32];
                snprintf(buf, sizeof(buf), "Recorded %.1f s", secs);
                drawStatus(buf, "B to play");
                Serial.printf("Recording stopped: %u bytes (%.1f s)\n", recBytes, secs);
                logSampleStats();
                break;
            }
            // Read a chunk from the mic into PSRAM
            const size_t CHUNK = 512;
            if (recBytes + CHUNK <= AUDIO_BUF_BYTES) {
                size_t got = 0;
                i2s_channel_read(rx_chan, (uint8_t *)audioBuf + recBytes,
                                 CHUNK, &got, pdMS_TO_TICKS(20));
                recBytes += got;
            } else {
                // Buffer full — auto-stop
                state = IDLE;
                drawStatus("Buffer full", "B to play");
                Serial.println("Buffer full, stopped recording");
            }
            break;
        }

        case PLAYING: {
            const size_t CHUNK = 512;
            size_t remaining = recBytes - playPos;
            if (remaining == 0) {
                state = IDLE;
                drawStatus("Done", "B to play again");
                Serial.println("Playback done");
                break;
            }
            size_t toWrite = remaining < CHUNK ? remaining : CHUNK;
            size_t written = 0;
            i2s_channel_write(tx_chan, (uint8_t *)audioBuf + playPos,
                              toWrite, &written, pdMS_TO_TICKS(100));
            playPos += written;
            break;
        }
    }

    delay(1);
}
