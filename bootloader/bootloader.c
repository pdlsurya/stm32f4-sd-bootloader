/*
 * flashProgram.c
 *
 *  Created on: Dec 31, 2023
 *      Author: pdlsurya
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "bootloader.h"
#include "sdFat32.h"

#define HEX_RECORD_ASCII_LEN 43
#define HEX_RECORD_HEX_LEN 21

static uint32_t writeCounter;
static uint32_t writeAddressHi = 0x00000000; // upper 16-bits of the write address.
static file appHexFile;
static uint32_t byteCnt;
static uint8_t recBufASCII[HEX_RECORD_ASCII_LEN];
static uint8_t recBufHEX[HEX_RECORD_HEX_LEN];
static bool erased = false;
static flashSector_t flashSectors[] = { { 0x08000000, 0x08003FFF }, // sector 0
		{ 0x08004000, 0x08007FFF }, // sector 1
		{ 0x08008000, 0x0800BFFF }, // sector 2
		{ 0x0800C000, 0x0800FFFF }, // sector 3
		{ 0x08010000, 0x0801FFFF }, // sector 4
		{ 0x08020000, 0x0803FFFF }, // sector 5
		{ 0x08040000, 0x0805FFFF }, // sector 6
		{ 0x08060000, 0x0807FFFF }, // sector 7
		{ 0x08080000, 0x0809FFFF }, // sector 8
		{ 0x080A0000, 0x080BFFFF }, // sector 9
		{ 0x080C0000, 0x080DFFFF }, // sector 10
		{ 0x080E0000, 0x080FFFFF } // sector 11
};

/**
 * Reads a single ASCII hex record from the hex file. The record is returned in @p recBufASCII.
 * The length of the record is returned.
 * @return the length of the record.
 */
static uint8_t getHexRecordASCII(void) {
	// Clear the buffer
	memset(recBufASCII, 0, sizeof(recBufASCII));
	uint8_t index = 0;
	while (byteCnt < fileSize(&appHexFile)) {
		// Read a single byte from the file
		uint8_t ch = fileReadByte(&appHexFile);

		byteCnt++;

		// Skip \r (carriage return)
		if (ch == '\r') {
			continue;
		}
		// Stop reading at the end of the line
		if (ch == '\n') {
			break;
		}

		// Store the byte in the buffer
		recBufASCII[index++] = ch;

	}
	return index;
}

/**
 * Converts a single ASCII hex byte (two characters) to a binary byte.
 * @param hexByteASCII The two character ASCII hex byte.
 * @return The binary byte.
 */
static uint8_t getHexByte(uint8_t *hexByteASCII) {
	uint8_t hexVal = 0;
	// Loop through each character of the hex byte
	for (uint8_t i = 0; i < 2; i++) {
		// Check if the character is a letter (A-F)
		if (hexByteASCII[i] >= 'A' && hexByteASCII[i] <= 'F') {
			// Calculate the value of the letter (0-9 for A-F)
			hexVal |= (hexByteASCII[i] - 55) << (4 * (1 - i));
		} else {
			// Calculate the value of the number (0-9)
			hexVal |= (hexByteASCII[i] - 48) << (4 * (1 - i));
		}
	}
	return hexVal;
}

/**
 * Converts ASCII hex record from recBufASCII to binary and stores it in recBufHEX.
 * @param len The length of the ASCII hex record.
 * @return The number of bytes converted and stored in recBufHEX.
 */
static uint8_t getHexRecordHEX(uint8_t len) {
	// Clear the binary buffer
	memset(recBufHEX, 0, sizeof(recBufHEX));

	uint8_t index = 0;
	// Convert each pair of ASCII hex characters to a binary byte
	for (uint8_t i = 1; i < len; i += 2) {
		recBufHEX[index++] = getHexByte(&recBufASCII[i]);
	}

	return index;
}

/**
 * Calculates the checksum of a single hex record. The checksum is stored in the last byte of the record.
 * @param recordLen The length of the record.
 * @return The checksum of the record.
 */
static uint8_t getCheckSum(uint8_t recordLen) {
	// Initialize the result to 0
	uint8_t res = 0;

	// Loop through each byte of the record except the checksum
	for (int i = 0; i < recordLen - 1; i++) {
		// Add the byte to the result
		res += recBufHEX[i];
	}

	// Calculate the checksum by inverting the result and adding 1
	return (~res + 1);
}

/**
 * Converts the binary hex record in recBufHEX to a struct.
 * @param recordLen The length of the binary hex record.
 * @return The struct representation of the record.
 */
static hexRecord_t getRecordStruct(uint8_t recordLen) {
	hexRecord_t record = { 0 };
	// The length of the record is stored in the first byte
	record.length = recBufHEX[0];

	// The address of the record is stored in the second and third bytes
	record.address = recBufHEX[2];
	record.address |= ((uint16_t) recBufHEX[1]) << 8;

	// The type of the record is stored in the fourth byte
	record.type = recBufHEX[3];

	// The data of the record is stored in the fifth to (length+5)th bytes
	memcpy(record.data, &recBufHEX[4], record.length);

	// The checksum of the record is stored in the last byte
	record.checksum = recBufHEX[recordLen - 1];

	return record;
}

/**
 * Calculates the number of sectors to erase in the flash memory for a given image size and start address.
 * @param imageSize The size of the image in bytes.
 * @param startAddress The start address of the image in the flash memory.
 * @return The number of sectors to erase.
 */
static uint8_t getEraseSectorsCount(uint32_t imageSize, uint32_t startAddress) {
	// Calculate the end address of the image
	uint32_t endAddress = startAddress + imageSize - 1;

	// Initialize the number of sectors to erase
	uint8_t sectorsToErase = 0;

	// Loop through each sector and check if the image overlaps with it
	for (int i = 0; i < (sizeof(flashSectors) / sizeof(flashSectors[0])); i++) {
		// Check if the image overlaps with the current sector
		if ((endAddress >= flashSectors[i].startAddress
				&& startAddress <= flashSectors[i].endAddress)) {
			// Increment the number of sectors to erase
			sectorsToErase++;
		}
	}

	// Return the number of sectors to erase
	return sectorsToErase;
}

/**
 * @brief Calculates the sector number for a given address in the flash memory.
 * @param address The address in the flash memory.
 * @return The sector number  where the address is located, or 0xFFFFFFFF if not found.
 */
static uint32_t getStartSector(uint32_t address) {
	for (uint32_t i = 0; i < (sizeof(flashSectors) / sizeof(flashSectors[0]));
			i++) {
		if (address >= flashSectors[i].startAddress
				&& address <= flashSectors[i].endAddress) {
			return i;
		}
	}
	return 0xFFFFFFFF; // Invalid address
}

/**
 * @brief Erases the sectors of the flash memory where the new image is going to be written.
 * @param startAddress The start address of the new image.
 * @return The HAL status of the erase operation.
 */
static HAL_StatusTypeDef eraseFlash(uint32_t startAddress) {
	FLASH_EraseInitTypeDef EraseInit;
	uint32_t SectorError;
	uint32_t hexFileSize = fileSize(&appHexFile);

	uint32_t imageSize = (uint32_t) ((double) hexFileSize / 2.8); // Approximate size of the flash image
	// Calculate the start sector of the new image
	uint8_t startSector = getStartSector(startAddress);

	// Initialize the erase configuration
	EraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
	EraseInit.Sector = startSector;
	// Calculate the number of sectors to erase
	EraseInit.NbSectors = getEraseSectorsCount(imageSize, startAddress);
	EraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

	// Erase the sectors
	return HAL_FLASHEx_Erase(&EraseInit, &SectorError);
}

/**
 * @brief Programs the flash memory with the new image.
 * @return The HAL status of the program operation.
 */
static HAL_StatusTypeDef programFlash() {
	HAL_StatusTypeDef status = HAL_OK;

	// Loop until the end of the file is reached
	while (1) {
		// Read a single ASCII hex record from the file
		uint8_t lenASCII = getHexRecordASCII();
		// Convert the record from ASCII to binary
		uint8_t lenHEX = getHexRecordHEX(lenASCII);
		// Get the struct representation of the record
		hexRecord_t record = getRecordStruct(lenHEX);

		uint32_t writeAddress = 0x0;

		if (getCheckSum(lenHEX) == record.checksum) {
			// Handle the different types of records
			switch (record.type) {
			case TYPE_ELAR: {
				// Read the extended address from the record
				uint16_t extendedAddress = (uint16_t) record.data[1];
				extendedAddress |= (((uint16_t) record.data[0]) << 8);
				// Store the extended address in the write address
				writeAddressHi = (((uint32_t) extendedAddress) << 16);
				break;
			}
			case TYPE_ESAR: {
				// Read the extended address from the record
				uint16_t extendedAddress = (uint16_t) record.data[1];
				extendedAddress |= (((uint16_t) record.data[0]) << 8);
				// Store the extended address in the write address
				writeAddressHi = (((uint32_t) extendedAddress) << 4);
				break;
			}
			case TYPE_DATA: {
				// Calculate the write address from the record address
				writeAddress = writeAddressHi + ((uint32_t) record.address);

				// Erase the sectors if necessary
				if (!erased) {
					status = eraseFlash(writeAddress);
					if (status != HAL_OK) {
						HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_RESET);
						return status;
					}
					erased = true;
				}

				// Program the flash memory with the data from the record
				for (uint8_t i = 0; i < record.length; i += 4) {
					status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
							writeAddress, *((uint32_t*) &record.data[i]));
					if (status != HAL_OK) {
						HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_RESET);
						return status;
					}

					writeAddress += 4;

					writeCounter++;
					if (writeCounter % 250 == 0) {
						HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);	//Blink led to indicated flash write
					}
				}
				break;
			}
			case TYPE_SLAR: {
				break;
			}

			case TYPE_EOF: {
				HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_SET);
				// Return the status when the end of the file is reached
				return status;
				break;
			}
			}
		} else {
			// Return an error if the checksum is incorrect
			HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_RESET);
			return HAL_ERROR;
		}
	}

	return status;
}

/**
 * @brief Check if a firmware update is available in the SD card.
 *
 * @return true if a firmware update is available, false otherwise.
 */
bool firmwareUpdateAvailable() {
	// Open the app.hex file in read mode
	appHexFile = fileOpen("/STM32-BOOT", "app.hex", FILE_MODE_READ);

	// Check if the file is valid
	if (!fileIsValid(&appHexFile)) {
		return false;
	}
	return true;
}

/**
 * @brief Process the firmware update.
 *
 * This function reads the firmware update from the hex file in the SD card and
 * programs it to the flash memory. It also locks the flash memory after the
 * programming is complete.
 *
 * @return The HAL status of the programming operation.
 */
HAL_StatusTypeDef bootloaderProcess() {
	HAL_StatusTypeDef status = HAL_OK;

	// Unlock the flash memory to enable programming
	status = HAL_FLASH_Unlock();

	if (status != HAL_OK) {
		// Return an error if the unlocking fails
		return status;

	}

	// Program the flash memory with the firmware update
	status = programFlash();

	if (status != HAL_OK) {
		//HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
		// Return an error if the programming fails
		return status;
	}

	// Lock the flash memory after the programming is complete
	HAL_FLASH_Lock();

	// Close the hex file
	fileClose(&appHexFile);

	// Delete the hex file after the programming is complete
	fileDelete("/STM32-BOOT", "app.hex");

	return status;
}

/**
 * @brief Initializes the bootloader by setting up the FAT32 file system.
 *
 * @return true if the initialization is successful, false otherwise.
 */
bool bootloaderInit() {
	// Initialize the FAT32 file system
	if (!sdFat32Init()) {
		// Return false if initialization fails
		return false;
	}

	// Return true if initialization is successful
	return true;
}
