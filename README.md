# STM32F4 SD Bootloader

A minimal bootloader for STM32F4 microcontrollers that reads Intel HEX firmware files from an SD card and programs them into sector-based flash memory. This bootloader supports FAT32 file systems and includes logic for extended linear addresses, checksum verification, and automatic sector erasure.

## Features

- Parses Intel HEX format from SD card
- Supports sector-based flash erasure and programming
- Validates records using checksum
- Handles extended linear addressing (ELAR/ESAR)
- FAT32 SD card support via custom file abstraction
- Toggles LED on PC13 to indicate flash programming progress
- Deletes the HEX file after a successful update

## Usage

1. Format your SD card with FAT32.
2. Place the `app.hex` file into a folder named `STM32-BOOT` on the SD card.
3. Insert the SD card into the STM32F4-based board.
4. Reset or power-cycle the board. The bootloader will automatically:
   - Initialize the SD card
   - Search for a valid `app.hex`
   - Erase necessary flash sectors
   - Program the firmware
   - Delete `app.hex` after success

## Flash Sector Map

The bootloader uses a predefined sector map from Sector 0 to Sector 11 (0x08000000 - 0x080FFFFF). Ensure the firmware address range falls within this region.

## Dependencies

This project depends on the [`sdFat32`](https://github.com/pdlsurya/sdFat32) library. It is included as a Git submodule under `lib/sdFat32`.

To clone this repository with submodules:

```bash
git clone --recurse-submodules https://github.com/pdlsurya/stm32f4-sd-bootloader.git
```

## License

MIT License – feel free to use and modify this bootloader for your projects.
