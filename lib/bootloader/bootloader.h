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

	HAL_StatusTypeDef bootloaderProcess();

	bool bootloaderInit();

	bool firmwareUpdateAvailable();

	uint32_t getAppStartAddress();

#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H_ */
