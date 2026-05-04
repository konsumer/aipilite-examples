These are various exmaples for AIPI-Lite AI Robot. If you want micropython on it, check out [this](https://github.com/bcarroll/aipi-lite).

## hardware

You can read more [here](https://www.robertlipe.com/449-2/) and I [documented](HARDWARE.md) it


## purchasing

- [Amazon](https://www.amazon.com/dp/B0FQNK543G)
- [AIPI](https://aipi.com/products/aipi-lite)


## dump

You might want to keep the original firmware:

```sh
# install tools
pip install esptool

# rip firmware
esptool -b 460800 read-flash 0 0x400000 aipi_lite_backup.bin

# erase
esptool erase_flash

# write firmware
esptool -b 460800 write-flash 0 aipi_lite_backup.bin
```

## bootloader mode

This is sometimes needed to program it, and it's a bit tricky. The screen will stay black, when it's working.

- SOmetimes you can get it into this mode by just unplugging it, and quickly plugging and programming it (`pio run -t upload -e display`)

- Remove 4 screws on the back of the device, then press the button under the display while plugging the device into a USB port.


## using this repo

All the demos are setup in [platformio.ini](./platformio.ini).

```sh
# install platformio
pip install platformio

# compile & upload basic demo
pip run -t upload -e basic
```
