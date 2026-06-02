/*
 * MIT License
 *
 * Copyright (c) 2025 Surya Poudel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef __BOOTLOADER_H_
#define __BOOTLOADER_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SET_STACK_TOP(appStartAddress) (__set_MSP((*((volatile uint32_t *)appStartAddress))))

#define APP_RESET_HANDLER(appStartAddress) ((reset_handler_t)(*((volatile uint32_t *)(appStartAddress + 4))))()

#define APP_START(appStartAddress)          \
	do                                      \
	{                                       \
		SET_STACK_TOP(appStartAddress);     \
		__DSB();                            \
		__ISB();                            \
		APP_RESET_HANDLER(appStartAddress); \
	} while (0);

	typedef void (*reset_handler_t)(void);

	typedef struct
	{
		uint32_t startAddress; ///< Start address of the flash sector
		uint32_t endAddress;   ///< End address of the flash sector
	} flashSector_t;

	typedef enum
	{
		TYPE_DATA,
		TYPE_EOF,
		TYPE_ESAR,
		TYPE_ELAR = 0x04,
		TYPE_SLAR = 0x05
	} recordType_t;

	typedef struct
	{
		uint16_t address;
		uint8_t data[16];
		uint8_t length;
		uint8_t checksum;
		recordType_t type;
	} hexRecord_t;

	/**
	 * @brief Programs the firmware image from the SD card into flash.
	 *
	 * The function erases the destination flash sectors, writes the new image,
	 * stores the application start address, and removes the processed HEX file.
	 *
	 * @return HAL status of the update operation.
	 */
	HAL_StatusTypeDef updateFirmware();

	/**
	 * @brief Initializes the bootloader storage layer.
	 *
	 * @return true when the SD/FAT32 stack is ready, false otherwise.
	 */
	bool bootloaderInit();

	/**
	 * @brief Checks whether a firmware image is present on the SD card.
	 *
	 * @return true when `app.hex` is available, false otherwise.
	 */
	bool firmwareUpdateAvailable();

	/**
	 * @brief Returns the stored application start address.
	 *
	 * If the bootloader has not already cached the address during an update,
	 * this reads it from `app.addr` on the SD card.
	 *
	 * @return Application start address.
	 */
	uint32_t getAppStartAddress();

#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H_ */
