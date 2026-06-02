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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "bootloader.h"
#include "sdFat32.h"
#include "main.h"

#define BOOTLOADER_END_ADDRESS 0x08003FFF
#define HEX_RECORD_ASCII_LEN 43
#define HEX_RECORD_HEX_LEN 21

static uint32_t appStartAddress = 0x00000000;
static uint32_t writeCounter;
static uint32_t writeAddressHi = 0x00000000;
static file appHexFile;
static file appAddrFile;
static uint32_t byteCnt;
static uint8_t recBufASCII[HEX_RECORD_ASCII_LEN];
static uint8_t recBufHEX[HEX_RECORD_HEX_LEN];
static bool erased = false;
static flashSector_t flashSectors[] = {
	{0x08000000, 0x08003FFF},
	{0x08004000, 0x08007FFF},
	{0x08008000, 0x0800BFFF},
	{0x0800C000, 0x0800FFFF},
	{0x08010000, 0x0801FFFF},
	{0x08020000, 0x0803FFFF},
	{0x08040000, 0x0805FFFF},
	{0x08060000, 0x0807FFFF},
	{0x08080000, 0x0809FFFF},
	{0x080A0000, 0x080BFFFF},
	{0x080C0000, 0x080DFFFF},
	{0x080E0000, 0x080FFFFF}
};

extern void Error_Handler();

static uint8_t getHexRecordASCII(void)
{
	memset(recBufASCII, 0, sizeof(recBufASCII));
	uint8_t index = 0;
	uint8_t ch;
	while (byteCnt < fileSize(&appHexFile))
	{
		fileRead(&appHexFile, &ch, 1);
		byteCnt++;
		if (ch == '\r')
		{
			continue;
		}
		if (ch == '\n')
		{
			break;
		}
		recBufASCII[index++] = ch;
	}
	return index;
}

static uint8_t getHexByte(uint8_t *hexByteASCII)
{
	uint8_t hexVal = 0;
	for (uint8_t i = 0; i < 2; i++)
	{
		if (hexByteASCII[i] >= 'A' && hexByteASCII[i] <= 'F')
		{
			hexVal |= (hexByteASCII[i] - 55) << (4 * (1 - i));
		}
		else
		{
			hexVal |= (hexByteASCII[i] - 48) << (4 * (1 - i));
		}
	}
	return hexVal;
}

static uint8_t getHexRecordHEX(uint8_t len)
{
	memset(recBufHEX, 0, sizeof(recBufHEX));
	uint8_t index = 0;
	for (uint8_t i = 1; i < len; i += 2)
	{
		recBufHEX[index++] = getHexByte(&recBufASCII[i]);
	}
	return index;
}

static uint8_t getCheckSum(uint8_t recordLen)
{
	uint8_t res = 0;
	for (int i = 0; i < recordLen - 1; i++)
	{
		res += recBufHEX[i];
	}
	return (~res + 1);
}

static hexRecord_t getRecordStruct(uint8_t recordLen)
{
	hexRecord_t record = {0};
	record.length = recBufHEX[0];
	record.address = recBufHEX[2];
	record.address |= ((uint16_t)recBufHEX[1]) << 8;
	record.type = recBufHEX[3];
	memcpy(record.data, &recBufHEX[4], record.length);
	record.checksum = recBufHEX[recordLen - 1];
	return record;
}

static uint8_t getEraseSectorsCount(uint32_t imageSize, uint32_t startAddress)
{
	uint32_t endAddress = startAddress + imageSize - 1;
	uint8_t sectorsToErase = 0;
	for (int i = 0; i < (sizeof(flashSectors) / sizeof(flashSectors[0])); i++)
	{
		if ((endAddress >= flashSectors[i].startAddress && startAddress <= flashSectors[i].endAddress))
		{
			sectorsToErase++;
		}
	}
	return sectorsToErase;
}

static uint32_t getStartSector(uint32_t address)
{
	for (uint32_t i = 0; i < (sizeof(flashSectors) / sizeof(flashSectors[0]));
		 i++)
	{
		if (address >= flashSectors[i].startAddress && address <= flashSectors[i].endAddress)
		{
			return i;
		}
	}
	return 0xFFFFFFFF;
}

static inline bool appOverlapsBootloader(uint32_t startAddress)
{
	return startAddress <= BOOTLOADER_END_ADDRESS;
}

static HAL_StatusTypeDef eraseFlash(uint32_t startAddress)
{
	FLASH_EraseInitTypeDef EraseInit;
	uint32_t SectorError;
	uint32_t hexFileSize = fileSize(&appHexFile);
	/* Intel HEX text is larger than the programmed image; 2.8 is an empirical factor. */
	uint32_t imageSize = (uint32_t)((double)hexFileSize / 2.8);
	uint8_t startSector = getStartSector(startAddress);
	EraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
	EraseInit.Sector = startSector;
	EraseInit.NbSectors = getEraseSectorsCount(imageSize, startAddress);
	EraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	return HAL_FLASHEx_Erase(&EraseInit, &SectorError);
}

static HAL_StatusTypeDef programFlash()
{
	HAL_StatusTypeDef status = HAL_OK;
	while (1)
	{
		uint8_t lenASCII = getHexRecordASCII();
		uint8_t lenHEX = getHexRecordHEX(lenASCII);
		hexRecord_t record = getRecordStruct(lenHEX);
		uint32_t writeAddress = 0x0;
		if (getCheckSum(lenHEX) == record.checksum)
		{
			switch (record.type)
			{
			case TYPE_ELAR:
			{
				uint16_t extendedAddress = (uint16_t)record.data[1];
				extendedAddress |= (((uint16_t)record.data[0]) << 8);
				writeAddressHi = (((uint32_t)extendedAddress) << 16);
				break;
			}
			case TYPE_ESAR:
			{
				uint16_t extendedAddress = (uint16_t)record.data[1];
				extendedAddress |= (((uint16_t)record.data[0]) << 8);
				writeAddressHi = (((uint32_t)extendedAddress) << 4);
				break;
			}
			case TYPE_DATA:
			{
				writeAddress = writeAddressHi + ((uint32_t)record.address);
				if (!erased)
				{
					if (appOverlapsBootloader(writeAddress))
					{
						return HAL_ERROR;
					}
					status = eraseFlash(writeAddress);
					if (status != HAL_OK)
					{
						HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
						return status;
					}
					erased = true;
					appStartAddress = writeAddress;
				}
				for (uint8_t i = 0; i < record.length; i += 4)
				{
					status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
											   writeAddress, *((uint32_t *)&record.data[i]));
					if (status != HAL_OK)
					{
						HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
						return status;
					}

					writeAddress += 4;

					writeCounter++;
					if (writeCounter % 200 == 0)
					{
						HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
					}
				}
				break;
			}
			case TYPE_SLAR:
			{
				break;
			}
			case TYPE_EOF:
			{
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
				return status;
			}
			}
		}
		else
		{
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
			return HAL_ERROR;
		}
	}
	return status;
}

bool firmwareUpdateAvailable()
{
	appHexFile = fileOpen("/STM32-BOOT", "app.hex", FA_READ);
	if (!fileIsValid(&appHexFile))
	{
		return false;
	}
	return true;
}

HAL_StatusTypeDef updateFirmware()
{
	HAL_StatusTypeDef status = HAL_OK;
	status = HAL_FLASH_Unlock();
	if (status != HAL_OK)
	{
		return status;
	}
	status = programFlash();
	if (status != HAL_OK)
	{
		return status;
	}
	appAddrFile = fileOpen("/STM32-BOOT", "app.addr", FA_WRITE);
	if (!fileIsValid(&appAddrFile))
	{
		return HAL_ERROR;
	}
	if (!fileWrite(&appAddrFile, (uint8_t *)&appStartAddress, 4))
	{
		return HAL_ERROR;
	}
	fileClose(&appAddrFile);
	HAL_FLASH_Lock();
	fileClose(&appHexFile);
	fileDelete("/STM32-BOOT", "app.hex");
	return status;
}

uint32_t getAppStartAddress()
{
	if (appStartAddress == 0)
	{
		appAddrFile = fileOpen("/STM32-BOOT", "app.addr", FA_READ);
		if (!fileIsValid(&appAddrFile))
		{
			Error_Handler();
		}
		if (!fileRead(&appAddrFile, (uint8_t *)&appStartAddress, 4))
		{
			Error_Handler();
		}
		fileClose(&appAddrFile);
	}
	return appStartAddress;
}

bool bootloaderInit()
{
	if (!sdFat32Init())
	{
		return false;
	}
	return true;
}
