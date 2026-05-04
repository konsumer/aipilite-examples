# AIPI-Lite / XOrigin AI Pi Lite Hardware Reference

## Chip

**ESP32-S3** (QFN56, rev0)
- Dual-core 240 MHz
- 16 MB flash
- 8 MB PSRAM (add `-DBOARD_HAS_PSRAM` to build flags to enable)
- Wi-Fi + BT 5 LE

## Power

**GPIO 10 — PWR_CTL** must be driven HIGH immediately on startup or the power management circuit will cut system power. Every sketch must include:

```cpp
pinMode(10, OUTPUT);
digitalWrite(10, HIGH);
```

| GPIO | Function    | Notes |
|------|-------------|-------|
| 10   | PWR_CTL     | Must be HIGH at boot |
| 2    | Battery ADC | ADC1 CH1, 12dB atten, ×2.0 scale |

## Buttons

| Button | GPIO | Notes |
|--------|------|-------|
| Left (A)  | 1  | Power + input, active LOW |
| Right (B) | 42 | User input, active LOW |

GPIO 1 is also ADC1 CH0 — the original firmware reads it via ADC.

## Display — ST7735, 128×128

| Signal | GPIO | Function |
|--------|------|----------|
| SCK    | 16   | SPI clock |
| MOSI   | 17   | SPI data |
| CS     | 15   | Chip select |
| DC     | 7    | Data/command |
| RST    | 18   | Reset |
| BL     | 3    | Backlight PWM |

Constructor (Arduino_GFX): `Arduino_ST7735(bus, 18, 3, false, 128, 128, 0, 0)`

## Audio — ES8311

| Signal       | GPIO | Notes |
|--------------|------|-------|
| I2C SDA      | 5    | Address 0x18 |
| I2C SCL      | 4    | |
| I2S MCLK     | 6    | Master clock out |
| I2S BCLK     | 14   | Bit clock |
| I2S LRCLK    | 12   | Word select |
| I2S DOUT     | 11   | ESP32 → codec (speaker) |
| I2S DIN      | 13   | Codec → ESP32 (mic) |
| Speaker amp  | 9    | Enable HIGH before audio |

Original firmware: 16-bit, 2-channel, 24000 Hz, ESP32 is I2S master, ES8311 is slave. Opus encoded, resampled to 16 kHz before Wi-Fi transmission.

## LED

| GPIO | Function | Notes |
|------|----------|-------|
| 46   | WS2812 addressable LED | single RGB LED |

## Pogo Pins (back of device)

| Label | Notes |
|-------|-------|
| TX    | UART TX |
| RX    | UART RX |
| IO40  | GPIO 40 |
| IO41  | GPIO 41 |

## Uploading

The built-in USB JTAG interface (VID:PID `303A:1001`) is on GPIO 19/20 — do not touch those pins. esptool 5.x required in PATH (platform's bundled 4.x is too old for this device).

If device is stuck in a boot loop, erase and reflash all partitions:

```sh
esptool erase_flash
esptool --chip esp32s3 write_flash 0x0 .pio/build/<env>/bootloader.bin 0x8000 .pio/build/<env>/partitions.bin 0x10000 .pio/build/<env>/firmware.bin
```
