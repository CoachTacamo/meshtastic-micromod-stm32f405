# Meshtastic on SparkFun MicroMod STM32 (STM32F405) + SX1276 LoRa

Unofficial port of Meshtastic firmware 2.7.26 (`v2.7.26.54e0d8d`) to the
[SparkFun MicroMod STM32 Processor](https://www.sparkfun.com/products/21326)
(STM32F405RGT6) with an SX1276 LoRa function board, such as an EBYTE E19-915M30S
module on a MicroMod Function Zero position.

This is **not** an upstream-supported target. It is a community port that has
been tested on one hardware setup (see below). Read [Limitations](#limitations)
before deploying it.

PlatformIO environment: `micromod_stm32f405_sx1276`

## What works

Tested on a Single Main Board v2.1 with the radio in the Function Zero slot:

- Full ARM build, USB DFU flash, and byte-for-byte flash readback.
- Meshtastic USB serial protobuf API and the `meshtastic` CLI.
- SX1276 receive and transmit; relaying an encrypted packet with the hop limit
  decremented and acknowledgment from the destination.
- Settings and node identity persist across reboot and USB power removal. They
  are stored in the last 1 MiB of the processor board's 16 MiB SPI flash.

Defaults are ROUTER role, US region and LongFast. No Bluetooth, Wi-Fi, display,
GPS or I2C sensors are included.

## Hardware

| Part | Notes |
|---|---|
| SparkFun MicroMod STM32 Processor | STM32F405RGT6, 12 MHz HSE, 1 MiB flash |
| MicroMod Main Board | Tested on Single Main Board v2.1 (`v21` on the back) |
| SX1276 LoRa module in Function Zero | Tested with an EBYTE E19-915M30S |

### Pin mapping

Pins are STM32duino digital indices for the SparkFun MicroMod F405 variant. They
follow the SparkFun Function Zero routing for Main Board v2.1.

| Function | MCU pin | Index |
|---|---|---|
| SCK | PA5 | 26 |
| MOSI | PA7 | 27 |
| MISO | PA6 | 28 |
| NSS | PC4 | 29 |
| RESET | PA8 | 7 |
| DIO0 | PC0 | 0 |
| DIO1 | PA0 | 8 |
| TX enable | PC6 | 2 |
| RX enable | PD2 | 6 |
| Status LED | PA15 | 32 |

- PC0 and PA0 share EXTI line 0. Only DIO0 has an interrupt attached.
- The installed STM32duino core wrongly aliases `PC0` to `PIN_A1`. Use the digital
  index (0) instead.
- Onboard W25Q128JV flash is on SPI3: SCK PC10, MISO PC11, MOSI PC12, CS PC3.
- Other Main Board revisions, other processor revisions, and other function slots
  have **not** been tested. Check your own routing before using them.

## Build

Requires [PlatformIO](https://platformio.org/). Build as with any Meshtastic target:

```sh
git clone -b micromod-stm32f405 https://github.com/CoachTacamo/meshtastic-micromod-stm32f405.git
cd meshtastic-micromod-stm32f405
pio run -e micromod_stm32f405_sx1276
```

The output is `.pio/build/micromod_stm32f405_sx1276/firmware-*.bin`. Builds are
not byte-for-byte reproducible.

## Flash

Flash the `.bin` over USB DFU with [dfu-util](https://dfu-util.sourceforge.net/).
This needs no debug probe.

1. Hold the BOOT button, tap RESET, then release BOOT. The board appears as
   `0483:df11`. Check with `dfu-util -l`.
2. Write the image to internal flash:

   ```sh
   dfu-util -d 0483:df11 -a 0 -s 0x08000000 -D firmware-micromod_stm32f405_sx1276-2.7.26.54e0d8d.bin
   ```

3. Tap RESET without BOOT. A USB serial port appears (`0483:5740`, usually
   `/dev/ttyACM0`).
4. Check it:

   ```sh
   meshtastic --port /dev/ttyACM0 --info
   ```

On Linux you may need a udev rule or `sudo` for the DFU device. To back up the
original flash first, read it with `dfu-util ... -s 0x08000000:0x100000 -U backup.bin`.
Flashing does not mass-erase and does not touch the external settings storage.

**Attach a suitable antenna before powering the board.** The node transmits
automatically after boot.

## Limitations

- **RF power is capped at 2 dBm at the SX1276 chip** in `RF95Interface`, and this
  cap holds after configuration changes. The E19-915M30S has a power amplifier, so
  the real output is higher and has **not been measured or calibrated**. You are
  responsible for staying within the rules for your region and antenna. Raising
  the cap needs a source change and RF measurement.
- Only one hardware setup has been tested. An isolated relay test, range test,
  24 to 72 hour stability test and watchdog fault recovery test are not done.
- Storage: if the external flash is non-blank but will not mount, boot stops
  rather than reformatting. Interrupted-write behavior is untested.
- The node database is limited to 40 entries, and the stack is 16 KiB. Peak heap
  and stack use under load are unmeasured.
- The port waits three seconds at startup so USB logs can be captured.
- Optional features (BLE, Wi-Fi, display, GPS, MQTT, many modules) are excluded
  to fit the MCU. Flash use is about 23% and RAM about 18%.

## Files added or changed

New: `src/platform/stm32f4/` (architecture, hardware RNG, IWDG feed, LittleFS
over the onboard SPI flash) and `variants/stm32f4/micromod/` (this directory:
`platformio.ini`, `variant.h`, `stm32f405.ld`).

Changed: `src/main.cpp`, `src/FSCommon.*`, `src/Power.cpp`, `src/memGet.cpp`,
`src/xmodem.h`, `src/mesh/CryptoEngine.cpp`, `src/mesh/HardwareRNG.cpp`,
`src/mesh/RF95Interface.cpp`, `src/mesh/Router.cpp`. These are small
`ARCH_STM32F4` branches.

## License

GPL-3.0, same as Meshtastic. The LittleFS wrappers under
`src/platform/stm32f4/` keep their original license headers.
