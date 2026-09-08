# ESP32-S3 CP2112 Emulator

This project emulates a **Silicon Labs CP2112 HID-to-SMBus adapter** using an
ESP32-S3. This allows the **DJI Battery Killer GUI by mixeysan** to communicate
with an ESP32-S3 as if a real CP2112 were connected.

The sketch is intended for communication with the DJI BMS, for example to reset
a DJI Spark battery. The ESP32-S3 provides the CP2112 USB HID interface and
forwards SMBus/I²C transfers to the connected BMS.

The project has been successfully tested with **DJI Spark** batteries using the
DJI Battery Killer GUI displaying **"compiled 13.06.2021"**.

## Features

- CP2112 USB HID emulation
- CP2112-compatible USB identification: `VID 10C4`, `PID EA90`
- SMBus/I²C transfers for read, write, and write/read operations
- Configurable bus speed through the GUI HID commands
- Serial debug output for USB, I²C, and transfer status

## Hardware and Wiring

| ESP32-S3 | Function |
| --- | --- |
| GPIO8 | I²C/SMBus SDA |
| GPIO9 | I²C/SMBus SCL |
| GND | Common ground with the BMS |

### Wiring Overview

![Wiring overview for the ESP32-S3 and DJI Spark battery](docs/verdrahtung.svg)

Both I²C lines must each be pulled up to `3.3 V` with a `4.7 kOhm` resistor:

| Connection | Wiring |
| --- | --- |
| SDA / D | ESP32-S3 `GPIO8` to battery contact `D` (Data), with `4.7 kOhm` to `3.3 V` |
| SCL / C | ESP32-S3 `GPIO9` to battery contact `C` (Clock), with `4.7 kOhm` to `3.3 V` |
| Ground | ESP32-S3 `GND` to battery contact `-` / `GND` |
| Supply | Connect battery contact `+` only as required by the intended circuit |

### DJI Spark Battery: Contact Side

![Schematic DJI Spark battery contact assignment](docs/spark-akku-pinout.svg)

In the orientation shown, the battery contact side has six contacts:
`C | - | + | + | - | D`. `C` denotes Clock (`SCL`) and `D` denotes Data
(`SDA`). Verify the contact side and polarity on your own battery with a meter
before connecting it. Never identify battery contacts by trial and error or by
short-circuiting them.

The pins are defined in the sketch:

```cpp
#define I2C_SDA 8
#define I2C_SCL 9
```

The default bus speed is `100 kHz`. Both I²C lines, SDA and SCL, must each be
pulled up to `3.3 V` with a `4.7 kOhm` resistor. Do not connect the ESP32-S3
directly to a voltage higher than its permitted GPIO voltage. Use an appropriate
level shifter for different voltage levels.

## Requirements

- ESP32-S3 board with an available native USB port
- Arduino IDE or Arduino CLI
- ESP32 Arduino Core **2.0.17**
- **TinyUSB** for native USB HID communication
- DJI Battery Killer GUI by **mixeysan**
- USB data cable

`USB.h`, `USBHID.h`, `Wire.h`, and `Arduino.h` are provided by the ESP32 Arduino
Core. USB HID uses TinyUSB; an ESP32 Arduino Core 2.0.17 installation normally
requires no additional libraries for this.

## Installation and Flashing

1. Download or clone this repository.
2. Open `DJI_Battery_Killer_ESP32-S3-CP2112_Emulator.ino` in the Arduino IDE.
3. Select the ESP32-S3 board you are using.
4. Select the correct USB port.
5. Compile and flash the sketch to the ESP32-S3.
6. Connect the ESP32-S3 to the computer via USB after flashing.

After startup, the device should appear as a CP2112-compatible HID device. The
DJI Battery Killer GUI can then use this USB device to communicate with the BMS.

## Serial Diagnostics

The sketch outputs diagnostic messages at `115200 baud` over the serial
interface. At startup, it reports the I²C pins and USB identification. HID
reports as well as read, write, and status operations are also logged.

If diagnostics are not required, disable them in the sketch with:

```cpp
#define DEBUG_SERIAL 0
```

## Usage Notes

- The I²C/SMBus address and other SMBus parameters are normally set by the GUI
  through the CP2112 HID commands.
- The ESP32-S3 must receive stable USB power during communication.
- USB HID processing and transfer status are handled continuously by the sketch;
  avoid adding long delays to the main loop.

## Safety

Working with DJI batteries and their BMS can cause short circuits, fire, and
damage. Work only with suitable current limiting, insulated wiring, and
sufficient technical knowledge. Never operate the battery and BMS unattended,
and verify the pinout of the specific battery before connecting it.

## License

This repository currently contains no separate license file. Before
redistributing it, check the license terms of the original project and the DJI
Battery Killer GUI by mixeysan.
