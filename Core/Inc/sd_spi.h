#ifndef SD_SPI_H
#define SD_SPI_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/* Kích thước sector chuẩn của thẻ SD */
#define SD_SECTOR_SIZE 512U

typedef enum
{
    SD_CARD_UNKNOWN = 0,
    SD_CARD_SDSC,
    SD_CARD_SDHC
} SD_CardType;

/*
 * Khởi tạo thẻ SD ở chế độ SPI.
 *
 * Trả về:
 * true  : khởi tạo thành công
 * false : khởi tạo thất bại
 */
bool SD_Init(void);

/*
 * Kiểm tra thẻ đã được khởi tạo hay chưa.
 */
bool SD_IsReady(void);

/*
 * Đọc một hoặc nhiều sector.
 *
 * buffer: vùng nhớ nhận dữ liệu
 * sector: sector bắt đầu
 * count : số sector cần đọc
 */
bool SD_ReadBlocks(
    uint8_t *buffer,
    uint32_t sector,
    uint32_t count
);

/*
 * Ghi một hoặc nhiều sector.
 *
 * buffer: vùng dữ liệu cần ghi
 * sector: sector bắt đầu
 * count : số sector cần ghi
 */
bool SD_WriteBlocks(
    const uint8_t *buffer,
    uint32_t sector,
    uint32_t count
);

/*
 * Lấy tổng số sector của thẻ.
 *
 * Dung lượng thẻ:
 * sector_count × 512 byte
 */
uint32_t SD_GetSectorCount(void);

/*
 * Lấy loại thẻ hiện tại.
 */
SD_CardType SD_GetCardType(void);

#endif
