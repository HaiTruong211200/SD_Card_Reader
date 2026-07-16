#include "sd_spi.h"

extern SPI_HandleTypeDef hspi1;
static uint32_t sd_sector_count = 0U;

uint8_t SD_SPI_Transfer(uint8_t tx)
{
    uint8_t rx = 0xFF;

    HAL_SPI_TransmitReceive(
        &hspi1,
        &tx,
        &rx,
        1,
        HAL_MAX_DELAY
    );

    return rx;
}

uint8_t SD_TestCMD0(void)
{
    uint8_t response = 0xFF;

    uint8_t cmd0[6] =
    {
        0x40,
        0x00,
        0x00,
        0x00,
        0x00,
        0x95
    };

    /* CS HIGH */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    HAL_Delay(100);

    /* 80 clock */
    for (uint8_t i = 0; i < 10; i++)
    {
        SD_SPI_Transfer(0xFF);
    }

    /* Chọn thẻ */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    /* Gửi CMD0 */
    for (uint8_t i = 0; i < 6; i++)
    {
        SD_SPI_Transfer(cmd0[i]);
    }

    /* Chờ R1 */
    for (uint8_t i = 0; i < 10; i++)
    {
        response = SD_SPI_Transfer(0xFF);

        if (response != 0xFF)
        {
            break;
        }
    }

    /* Bỏ chọn thẻ */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    SD_SPI_Transfer(0xFF);

    return response;
}

uint8_t SD_TestCMD8(uint8_t r7[4])
{
    uint8_t response = 0xFF;

    uint8_t cmd8[6] =
    {
        0x48,   // 0x40 | CMD8
        0x00,
        0x00,
        0x01,
        0xAA,
        0x87
    };

    /* Chọn thẻ */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    /* Gửi CMD8 */
    for (uint8_t i = 0; i < 6; i++)
    {
        SD_SPI_Transfer(cmd8[i]);
    }

    /* Chờ phản hồi R1 */
    for (uint8_t i = 0; i < 10; i++)
    {
        response = SD_SPI_Transfer(0xFF);

        if (response != 0xFF)
        {
            break;
        }
    }

    /*
     * Nếu CMD8 hợp lệ, phản hồi là R7:
     * 1 byte R1 + 4 byte dữ liệu.
     */
    if (response == 0x01)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            r7[i] = SD_SPI_Transfer(0xFF);
        }
    }

    /* Bỏ chọn thẻ */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    /* Cấp thêm 8 clock */
    SD_SPI_Transfer(0xFF);

    return response;
}


uint8_t SD_SendCommandRaw(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t response = 0xFF;

    uint8_t packet[6];

    packet[0] = 0x40 | cmd;
    packet[1] = (uint8_t)(arg >> 24);
    packet[2] = (uint8_t)(arg >> 16);
    packet[3] = (uint8_t)(arg >> 8);
    packet[4] = (uint8_t)(arg);
    packet[5] = crc;

    for (uint8_t i = 0; i < 6; i++)
    {
        SD_SPI_Transfer(packet[i]);
    }

    for (uint8_t i = 0; i < 10; i++)
    {
        response = SD_SPI_Transfer(0xFF);

        if (response != 0xFF)
        {
            break;
        }
    }

    return response;
}

uint8_t SD_TestACMD41(void)
{
    uint8_t response55;
    uint8_t response41;

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    /* CMD55 */
    response55 = SD_SendCommandRaw(
        55,
        0x00000000,
        0x01
    );

    /*
     * ACMD41 với HCS = 1
     * Argument = 0x40000000
     */
    response41 = SD_SendCommandRaw(
        41,
        0x40000000,
        0x01
    );

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    SD_SPI_Transfer(0xFF);

    return response41;
}

uint8_t SD_TestWaitReady(void)
{
    uint8_t response = 0xFF;

    for (uint32_t i = 0; i < 1000; i++)
    {
        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_RESET
        );

        SD_SendCommandRaw(
            55,
            0x00000000,
            0x01
        );

        response = SD_SendCommandRaw(
            41,
            0x40000000,
            0x01
        );

        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFF);

        if (response == 0x00)
        {
            break;
        }

        HAL_Delay(1);
    }

    return response;
}

uint8_t SD_TestCMD58(uint8_t ocr[4])
{
    uint8_t response = 0xFF;

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    response = SD_SendCommandRaw(
        58,
        0x00000000,
        0x01
    );

    if (response == 0x00)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            ocr[i] = SD_SPI_Transfer(0xFF);
        }
    }

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    SD_SPI_Transfer(0xFF);

    return response;
}

uint8_t SD_TestReadSector(uint32_t sector, uint8_t buffer[512])
{
    uint8_t response;
    uint8_t token = 0xFF;

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    /*
     * Với SDHC/SDXC:
     * argument chính là số sector.
     */
    response = SD_SendCommandRaw(
        17,
        sector,
        0x01
    );

    if (response != 0x00)
    {
        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFF);

        return response;
    }

    /*
     * Chờ data token 0xFE.
     */
    for (uint32_t i = 0; i < 100000; i++)
    {
        token = SD_SPI_Transfer(0xFF);

        if (token == 0xFE)
        {
            break;
        }
    }

    if (token != 0xFE)
    {
        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFF);

        return 0xFF;
    }

    /*
     * Đọc 512 byte dữ liệu.
     */
    for (uint32_t i = 0; i < 512; i++)
    {
        buffer[i] = SD_SPI_Transfer(0xFF);
    }

    /*
     * Bỏ qua 2 byte CRC.
     */
    SD_SPI_Transfer(0xFF);
    SD_SPI_Transfer(0xFF);

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    SD_SPI_Transfer(0xFF);

    return 0x00;
}

#include "sd_spi.h"

extern SPI_HandleTypeDef hspi1;

static bool sd_initialized = false;
static SD_CardType sd_card_type = SD_CARD_UNKNOWN;


/* =========================================================
 * SPI transfer
 * ========================================================= */
//uint8_t SD_SPI_Transfer(uint8_t tx)
//{
//    uint8_t rx = 0xFFU;
//
//    HAL_SPI_TransmitReceive(
//        &hspi1,
//        &tx,
//        &rx,
//        1,
//        HAL_MAX_DELAY
//    );
//
//    return rx;
//}


/* =========================================================
 * Gửi một command tới SD card
 *
 * Trả về phản hồi R1.
 * Lưu ý: hàm này không tự điều khiển CS.
 * ========================================================= */
static uint8_t SD_SendCommand(
    uint8_t cmd,
    uint32_t arg,
    uint8_t crc
)
{
    uint8_t response = 0xFFU;

    uint8_t packet[6];

    packet[0] = 0x40U | cmd;
    packet[1] = (uint8_t)(arg >> 24);
    packet[2] = (uint8_t)(arg >> 16);
    packet[3] = (uint8_t)(arg >> 8);
    packet[4] = (uint8_t)arg;
    packet[5] = crc;

    /* Gửi command packet */
    for (uint8_t i = 0; i < 6U; i++)
    {
        SD_SPI_Transfer(packet[i]);
    }

    /* Chờ phản hồi R1 */
    for (uint8_t i = 0; i < 10U; i++)
    {
        response = SD_SPI_Transfer(0xFFU);

        if (response != 0xFFU)
        {
            break;
        }
    }

    return response;
}


/* =========================================================
 * Khởi tạo SD card ở SPI mode
 * ========================================================= */
bool SD_Init(void)
{
    uint8_t response;
    uint8_t r7[4];
    uint8_t ocr[4];

    sd_initialized = false;
    sd_card_type = SD_CARD_UNKNOWN;


    /* -----------------------------------------------------
     * 1. Bỏ chọn thẻ
     * ----------------------------------------------------- */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );


    /* -----------------------------------------------------
     * 2. Chờ nguồn ổn định
     * ----------------------------------------------------- */
    HAL_Delay(100U);


    /* -----------------------------------------------------
     * 3. Gửi ít nhất 74 clock khi CS HIGH
     *
     * 10 byte = 80 clock
     * ----------------------------------------------------- */
    for (uint8_t i = 0; i < 10U; i++)
    {
        SD_SPI_Transfer(0xFFU);
    }


    /* -----------------------------------------------------
     * 4. CMD0
     *
     * Đưa thẻ vào Idle State.
     * Mong đợi R1 = 0x01.
     * ----------------------------------------------------- */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    response = SD_SendCommand(
        0U,
        0x00000000UL,
        0x95U
    );

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    SD_SPI_Transfer(0xFFU);

    if (response != 0x01U)
    {
        return false;
    }


    /* -----------------------------------------------------
     * 5. CMD8
     *
     * Kiểm tra SD Version 2.
     * Argument = 0x000001AA
     * ----------------------------------------------------- */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    response = SD_SendCommand(
        8U,
        0x000001AAUL,
        0x87U
    );

    if (response != 0x01U)
    {
        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFFU);

        return false;
    }

    /* Đọc 4 byte còn lại của R7 */
    for (uint8_t i = 0; i < 4U; i++)
    {
        r7[i] = SD_SPI_Transfer(0xFFU);
    }

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    SD_SPI_Transfer(0xFFU);


    /* Kiểm tra voltage pattern + check pattern */
    if ((r7[2] != 0x01U) || (r7[3] != 0xAAU))
    {
        return false;
    }


    /* -----------------------------------------------------
     * 6. CMD55 + ACMD41
     *
     * Lặp đến khi thẻ thoát Idle State.
     *
     * ACMD41 argument:
     * 0x40000000 → HCS = 1
     * ----------------------------------------------------- */
    uint32_t start_tick = HAL_GetTick();

    do
    {
        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_RESET
        );

        /* CMD55 */
        response = SD_SendCommand(
            55U,
            0x00000000UL,
            0x01U
        );

        /*
         * CMD55 khi card còn Idle thường trả 0x01.
         * Ta không cần fail ngay nếu vẫn đang Idle.
         */

        /* ACMD41 */
        response = SD_SendCommand(
            41U,
            0x40000000UL,
            0x01U
        );

        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFFU);

        if (response == 0x00U)
        {
            break;
        }

        HAL_Delay(1U);

    } while ((HAL_GetTick() - start_tick) < 1000U);


    if (response != 0x00U)
    {
        return false;
    }


    /* -----------------------------------------------------
     * 7. CMD58
     *
     * Đọc OCR để xác định loại thẻ.
     * ----------------------------------------------------- */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    response = SD_SendCommand(
        58U,
        0x00000000UL,
        0x01U
    );

    if (response != 0x00U)
    {
        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFFU);

        return false;
    }


    /* Đọc 4 byte OCR */
    for (uint8_t i = 0; i < 4U; i++)
    {
        ocr[i] = SD_SPI_Transfer(0xFFU);
    }


    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    SD_SPI_Transfer(0xFFU);


    /* -----------------------------------------------------
     * 8. Kiểm tra CCS
     *
     * OCR bit 30 = CCS
     * Trong byte đầu tiên tương ứng bit 6.
     * ----------------------------------------------------- */
    if ((ocr[0] & 0x40U) != 0U)
    {
        sd_card_type = SD_CARD_SDHC;
    }
    else
    {
        sd_card_type = SD_CARD_SDSC;
    }


    /* -----------------------------------------------------
     * 9. Khởi tạo thành công
     * ----------------------------------------------------- */
    sd_initialized = true;

    return true;
}


/* =========================================================
 * Kiểm tra trạng thái khởi tạo
 * ========================================================= */
bool SD_IsReady(void)
{
    return sd_initialized;
}


/* =========================================================
 * Lấy loại thẻ
 * ========================================================= */
SD_CardType SD_GetCardType(void)
{
    return sd_card_type;
}

bool SD_ReadBlocks(
    uint32_t sector,
    uint8_t *buffer,
    uint32_t count
)
{
    uint8_t response;
    uint8_t token;

    if (!sd_initialized)
    {
        return false;
    }

    if ((buffer == NULL) || (count == 0U))
    {
        return false;
    }

    for (uint32_t block = 0; block < count; block++)
    {
        token = 0xFFU;

        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_RESET
        );

        /*
         * CMD17: Read Single Block
         *
         * Với SDHC:
         * argument = sector number
         */
        response = SD_SendCommand(
            17U,
            sector + block,
            0x01U
        );

        if (response != 0x00U)
        {
            HAL_GPIO_WritePin(
                SD_CS_GPIO_Port,
                SD_CS_Pin,
                GPIO_PIN_SET
            );

            SD_SPI_Transfer(0xFFU);

            return false;
        }

        /*
         * Chờ data token 0xFE
         */
        uint32_t start_tick = HAL_GetTick();

        do
        {
            token = SD_SPI_Transfer(0xFFU);

            if (token == 0xFEU)
            {
                break;
            }

        } while ((HAL_GetTick() - start_tick) < 100U);

        if (token != 0xFEU)
        {
            HAL_GPIO_WritePin(
                SD_CS_GPIO_Port,
                SD_CS_Pin,
                GPIO_PIN_SET
            );

            SD_SPI_Transfer(0xFFU);

            return false;
        }

        /*
         * Đọc 512 byte
         */
        for (uint32_t i = 0; i < 512U; i++)
        {
            buffer[(block * 512U) + i]
                = SD_SPI_Transfer(0xFFU);
        }

        /*
         * Bỏ qua 2 byte CRC
         */
        SD_SPI_Transfer(0xFFU);
        SD_SPI_Transfer(0xFFU);

        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFFU);
    }

    return true;
}

bool SD_WriteBlocks(
    uint32_t sector,
    const uint8_t *buffer,
    uint32_t count
)
{
    uint8_t response;
    uint8_t data_response;

    if (!sd_initialized)
    {
        return false;
    }

    if ((buffer == NULL) || (count == 0U))
    {
        return false;
    }

    for (uint32_t block = 0; block < count; block++)
    {
        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_RESET
        );

        /*
         * CMD24: Write Single Block
         */
        response = SD_SendCommand(
            24U,
            sector + block,
            0x01U
        );

        if (response != 0x00U)
        {
            HAL_GPIO_WritePin(
                SD_CS_GPIO_Port,
                SD_CS_Pin,
                GPIO_PIN_SET
            );

            SD_SPI_Transfer(0xFFU);

            return false;
        }

        /*
         * Gửi 1 byte dummy trước data token.
         */
        SD_SPI_Transfer(0xFFU);

        /*
         * Data token cho single block write.
         */
        SD_SPI_Transfer(0xFEU);

        /*
         * Gửi 512 byte dữ liệu.
         */
        for (uint32_t i = 0; i < 512U; i++)
        {
            SD_SPI_Transfer(
                buffer[(block * 512U) + i]
            );
        }

        /*
         * CRC dummy.
         */
        SD_SPI_Transfer(0xFFU);
        SD_SPI_Transfer(0xFFU);

        /*
         * Đọc Data Response Token.
         *
         * 0bxxx00101 = data accepted
         */
        data_response = SD_SPI_Transfer(0xFFU);

        if ((data_response & 0x1FU) != 0x05U)
        {
            HAL_GPIO_WritePin(
                SD_CS_GPIO_Port,
                SD_CS_Pin,
                GPIO_PIN_SET
            );

            SD_SPI_Transfer(0xFFU);

            return false;
        }

        /*
         * Chờ thẻ ghi xong.
         * Trong lúc busy, MISO = 0x00.
         */
        uint32_t start_tick = HAL_GetTick();

        while (SD_SPI_Transfer(0xFFU) == 0x00U)
        {
            if ((HAL_GetTick() - start_tick) > 500U)
            {
                HAL_GPIO_WritePin(
                    SD_CS_GPIO_Port,
                    SD_CS_Pin,
                    GPIO_PIN_SET
                );

                SD_SPI_Transfer(0xFFU);

                return false;
            }
        }

        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFFU);
    }

    return true;
}

uint32_t SD_GetSectorCount(void)
{
    uint8_t response;
    uint8_t token = 0xFFU;
    uint8_t csd[16];

    uint32_t c_size;

    if (!sd_initialized)
    {
        return 0U;
    }

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    response = SD_SendCommand(
        9U,
        0x00000000UL,
        0x01U
    );

    if (response != 0x00U)
    {
        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFFU);

        return 0U;
    }

    uint32_t start_tick = HAL_GetTick();

    do
    {
        token = SD_SPI_Transfer(0xFFU);

        if (token == 0xFEU)
        {
            break;
        }

    } while ((HAL_GetTick() - start_tick) < 100U);

    if (token != 0xFEU)
    {
        HAL_GPIO_WritePin(
            SD_CS_GPIO_Port,
            SD_CS_Pin,
            GPIO_PIN_SET
        );

        SD_SPI_Transfer(0xFFU);

        return 0U;
    }

    for (uint8_t i = 0; i < 16U; i++)
    {
        csd[i] = SD_SPI_Transfer(0xFFU);
    }

    /* Bỏ qua CRC */
    SD_SPI_Transfer(0xFFU);
    SD_SPI_Transfer(0xFFU);

    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    SD_SPI_Transfer(0xFFU);

    /*
     * CSD Version 2.0:
     * C_SIZE nằm ở:
     * csd[7] bits 5:0
     * csd[8]
     * csd[9]
     */
    if ((csd[0] >> 6) == 1U)
    {
        c_size =
            ((uint32_t)(csd[7] & 0x3FU) << 16) |
            ((uint32_t)csd[8] << 8) |
            ((uint32_t)csd[9]);

        sd_sector_count =
            (c_size + 1U) * 1024U;

        return sd_sector_count;
    }

    return 0U;
}
