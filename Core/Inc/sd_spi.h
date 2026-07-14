#ifndef SD_SPI_H
#define SD_SPI_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define SD_SECTOR_SIZE 512U

typedef enum
{
    SD_CARD_UNKNOWN = 0,
    SD_CARD_SDSC,
    SD_CARD_SDHC
} SD_CardType;

bool SD_Init(void);
bool SD_IsReady(void);

bool SD_WriteBlocks(
    uint32_t sector,
    const uint8_t *buffer,
    uint32_t count
);

uint32_t SD_GetSectorCount(void);
SD_CardType SD_GetCardType(void);
uint8_t SD_TestCMD8(uint8_t r7[4]);
uint8_t SD_SendCommandRaw(uint8_t cmd, uint32_t arg, uint8_t crc);
uint8_t SD_TestACMD41(void);
uint8_t SD_TestWaitReady(void);
uint8_t SD_TestCMD58(uint8_t ocr[4]);
uint8_t SD_TestReadSector(uint32_t sector, uint8_t buffer[512]);

SD_CardType SD_GetCardType(void);

uint8_t SD_SPI_Transfer(uint8_t tx);

bool SD_ReadBlocks(
    uint32_t sector,
    uint8_t *buffer,
    uint32_t count
);




#endif
