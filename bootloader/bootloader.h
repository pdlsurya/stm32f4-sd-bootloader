/*
 * flashProgram.h
 *
 *  Created on: Dec 31, 2023
 *      Author: pdlsurya
 */

#ifndef __BOOTLOADER_H_
#define __BOOTLOADER_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

#define APP_START_ADDRESS 0x08004000

#define RESET_HANDLER_ADDRESS (*((volatile uint32_t *)(APP_START_ADDRESS + 4)))

#define STACK_TOP (*((volatile uint32_t *)APP_START_ADDRESS))

#define SECTOR_0 0U ///< Flash sector 0
#define SECTOR_1 1U ///< Flash sector 1
#define SECTOR_2 2U ///< Flash sector 2
#define SECTOR_3 3U ///< Flash sector 3
#define SECTOR_4 4U ///< Flash sector 4
#define SECTOR_5 5U ///< Flash sector 5
#define SECTOR_6 6U ///< Flash sector 6
#define SECTOR_7 7U ///< Flash sector 7

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

extern uint32_t app_start_address;

HAL_StatusTypeDef bootloaderProcess();

bool bootloaderInit();

bool firmwareUpdateAvailable();

#endif /* __BOOTLOADER_H_ */
