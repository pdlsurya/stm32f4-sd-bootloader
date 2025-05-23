
# STM32F4 SD Bootloader

A minimal bootloader for STM32F4 microcontrollers that reads Intel HEX firmware files from an SD card and programs them into sector-based flash memory. This bootloader supports FAT32 file systems and includes robust handling of extended addressing, checksum validation, and automatic flash erasure.

## Features

- Parses and flashes Intel HEX files from `/STM32-BOOT/app.hex`
- Supports Intel HEX record types: DATA, EOF, ELAR (0x04), ESAR (0x02), SLAR (0x05)
- Automatically erases only the flash sectors necessary for the image
- Validates checksum for each HEX record before programming
- Calculates real image size to optimize sector erasure
- FAT32 SD card access through a `sdFat32` file abstraction layer
- Dynamically saves the application's start address to `/STM32-BOOT/app.addr`, eliminating the need for hardcoded addresses and enabling flexible placement of the application in flash memory.
- Toggles LED on `GPIOC Pin 13` during flashing and error states
- Deletes `app.hex` after a successful update to prevent reprogramming

## Usage

1. Format your SD card with FAT32.
2. Place the `app.hex` file into a folder named `STM32-BOOT` at the root of the SD card.
3. Insert the SD card into your STM32F4-based board.
4. On power-up or reset, the bootloader will:
   - Initialize the SD card and FAT32 file system
   - Check for a valid `app.hex`
   - Erase necessary flash sectors based on the size of the firmware
   - Program the firmware to internal flash memory
   - Save the start address to `app.addr`
   - Delete `app.hex`
   - Light the LED to signal completion or turn off on error

## Flash Sector Map

The bootloader uses a fixed sector map covering:

```
0x08000000 - 0x08003FFF  (Sector 0)
0x08004000 - 0x08007FFF  (Sector 1)
...
0x080E0000 - 0x080FFFFF  (Sector 11)
```

Ensure that the firmware being flashed falls within these bounds.

## LED Status (GPIOC Pin 13)

- **Blinking**: Flash programming in progress
- **ON**: Flashing completed successfully
- **OFF**: Error during flashing (e.g., checksum or HAL failure)

## Dependencies

This bootloader depends on the [`sdFat32`](https://github.com/pdlsurya/sdFat32) library for FAT32 file access. Ensure it's included and integrated properly.

To clone this project with submodules:

```bash
git clone --recurse-submodules https://github.com/pdlsurya/stm32f4-sd-bootloader.git
```

## Functions Overview

| Function                   | Description                                                  |
|---------------------------|--------------------------------------------------------------|
| `bootloaderInit()`        | Initializes the SD and FAT32 system                          |
| `firmwareUpdateAvailable()` | Checks for presence of `app.hex`                             |
| `bootloaderProcess()`     | Erases flash, writes firmware, saves app address, deletes hex |
| `getAppStartAddress()`    | Loads the application start address from `app.addr`          |
| `APP_START(addr)`         | Jumps to application at specified address                    |

## License

MIT License – feel free to use, modify, and distribute this project.
