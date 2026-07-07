#include "sd_spi.h"

/*
 * Nếu project của bạn sử dụng SPI khác,
 * ví dụ SPI2, hãy đổi hspi1 thành hspi2.
 */
extern SPI_HandleTypeDef hspi1;

/* Các lệnh cơ bản của thẻ SD */
#define SD_CMD0    0U
#define SD_CMD8    8U
#define SD_CMD9    9U
#define SD_CMD12   12U
#define SD_CMD16   16U
#define SD_CMD17   17U
#define SD_CMD18   18U
#define SD_CMD23   23U
#define SD_CMD24   24U
#define SD_CMD25   25U
#define SD_CMD55   55U
#define SD_CMD58   58U

#define SD_ACMD41  41U

/* Token dữ liệu */
#define SD_TOKEN_START_BLOCK          0xFEU
#define SD_TOKEN_MULTI_WRITE          0xFCU
#define SD_TOKEN_STOP_MULTI_WRITE     0xFDU

/* Data response */
#define SD_DATA_ACCEPTED              0x05U

/* Timeout */
#define SD_SPI_TIMEOUT_MS             100U
#define SD_INIT_TIMEOUT_MS            2000U
#define SD_READ_TIMEOUT_MS            500U
#define SD_WRITE_TIMEOUT_MS           1000U

static bool sd_initialized = false;
static SD_CardType sd_card_type = SD_CARD_UNKNOWN;
static uint32_t sd_sector_count = 0U;

/* ============================================================
 * Điều khiển chân CS
 * ============================================================ */

static void SD_CS_Low(void)
{
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );
}

static void SD_CS_High(void)
{
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );
}

/* ============================================================
 * Hàm SPI cơ bản
 * ============================================================ */

static uint8_t SD_SPI_Transfer(uint8_t tx_data)
{
    uint8_t rx_data = 0xFFU;

    HAL_StatusTypeDef status;

    status = HAL_SPI_TransmitReceive(
        &hspi1,
        &tx_data,
        &rx_data,
        1U,
        SD_SPI_TIMEOUT_MS
    );

    if (status != HAL_OK)
    {
        return 0xFFU;
    }

    return rx_data;
}

static void SD_SPI_ReceiveBuffer(
    uint8_t *buffer,
    uint32_t length
)
{
    uint8_t dummy = 0xFFU;

    for (uint32_t i = 0; i < length; i++)
    {
        HAL_SPI_TransmitReceive(
            &hspi1,
            &dummy,
            &buffer[i],
            1U,
            SD_SPI_TIMEOUT_MS
        );
    }
}

static void SD_SPI_TransmitBuffer(
    const uint8_t *buffer,
    uint32_t length
)
{
    uint8_t rx_dummy;

    for (uint32_t i = 0; i < length; i++)
    {
        HAL_SPI_TransmitReceive(
            &hspi1,
            (uint8_t *)&buffer[i],
            &rx_dummy,
            1U,
            SD_SPI_TIMEOUT_MS
        );
    }
}

/* ============================================================
 * Chờ thẻ sẵn sàng
 * ============================================================ */

static bool SD_WaitReady(uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();

    do
    {
        if (SD_SPI_Transfer(0xFFU) == 0xFFU)
        {
            return true;
        }

    } while ((HAL_GetTick() - start_tick) < timeout_ms);

    return false;
}

/* ============================================================
 * Chọn và bỏ chọn thẻ
 * ============================================================ */

static bool SD_Select(void)
{
    SD_CS_Low();

    /*
     * Gửi một byte clock sau khi kéo CS xuống.
     */
    SD_SPI_Transfer(0xFFU);

    if (SD_WaitReady(500U))
    {
        return true;
    }

    SD_CS_High();
    SD_SPI_Transfer(0xFFU);

    return false;
}

static void SD_Deselect(void)
{
    SD_CS_High();

    /*
     * Cấp thêm 8 xung clock sau khi bỏ chọn thẻ.
     */
    SD_SPI_Transfer(0xFFU);
}

/* ============================================================
 * Tính CRC7 cho command
 *
 * CMD0 và CMD8 bắt buộc phải có CRC hợp lệ trước khi tắt CRC.
 * Để đơn giản, driver sử dụng CRC cố định cho hai lệnh này.
 * ============================================================ */

static uint8_t SD_GetCommandCRC(uint8_t command)
{
    if (command == SD_CMD0)
    {
        return 0x95U;
    }

    if (command == SD_CMD8)
    {
        return 0x87U;
    }

    return 0x01U;
}

/* ============================================================
 * Gửi command
 * ============================================================ */

static uint8_t SD_SendCommand(
    uint8_t command,
    uint32_t argument
)
{
    uint8_t response;

    /*
     * ACMD phải được đặt trước bởi CMD55.
     * Bit 7 được dùng nội bộ để đánh dấu ACMD.
     */
    if ((command & 0x80U) != 0U)
    {
        command &= 0x7FU;

        response = SD_SendCommand(SD_CMD55, 0U);

        if (response > 1U)
        {
            return response;
        }
    }

    /*
     * CMD12 là lệnh dừng truyền nhiều block.
     * Không bỏ chọn thẻ trước khi gửi CMD12.
     */
    if (command != SD_CMD12)
    {
        SD_Deselect();

        if (!SD_Select())
        {
            return 0xFFU;
        }
    }

    /*
     * Command packet gồm 6 byte:
     *
     * 0: 0x40 | command
     * 1-4: argument
     * 5: CRC
     */
    SD_SPI_Transfer((uint8_t)(0x40U | command));

    SD_SPI_Transfer(
        (uint8_t)(argument >> 24)
    );

    SD_SPI_Transfer(
        (uint8_t)(argument >> 16)
    );

    SD_SPI_Transfer(
        (uint8_t)(argument >> 8)
    );

    SD_SPI_Transfer(
        (uint8_t)argument
    );

    SD_SPI_Transfer(
        SD_GetCommandCRC(command)
    );

    /*
     * CMD12 có một byte stuff sau command.
     */
    if (command == SD_CMD12)
    {
        SD_SPI_Transfer(0xFFU);
    }

    /*
     * Chờ phản hồi R1.
     * Bit 7 bằng 0 nghĩa là response hợp lệ.
     */
    for (uint8_t retry = 0; retry < 10U; retry++)
    {
        response = SD_SPI_Transfer(0xFFU);

        if ((response & 0x80U) == 0U)
        {
            return response;
        }
    }

    return 0xFFU;
}

/* ============================================================
 * Nhận một block dữ liệu
 * ============================================================ */

static bool SD_ReceiveDataBlock(
    uint8_t *buffer,
    uint32_t length
)
{
    uint8_t token;
    uint32_t start_tick = HAL_GetTick();

    do
    {
        token = SD_SPI_Transfer(0xFFU);

        if (token == SD_TOKEN_START_BLOCK)
        {
            break;
        }

    } while ((HAL_GetTick() - start_tick) < SD_READ_TIMEOUT_MS);

    if (token != SD_TOKEN_START_BLOCK)
    {
        return false;
    }

    SD_SPI_ReceiveBuffer(buffer, length);

    /*
     * Bỏ qua hai byte CRC.
     */
    SD_SPI_Transfer(0xFFU);
    SD_SPI_Transfer(0xFFU);

    return true;
}

/* ============================================================
 * Gửi một block dữ liệu
 * ============================================================ */

static bool SD_TransmitDataBlock(
    const uint8_t *buffer,
    uint8_t token
)
{
    uint8_t response;

    if (!SD_WaitReady(SD_WRITE_TIMEOUT_MS))
    {
        return false;
    }

    SD_SPI_Transfer(token);

    /*
     * Token dừng ghi multi-block không có 512 byte dữ liệu.
     */
    if (token == SD_TOKEN_STOP_MULTI_WRITE)
    {
        return true;
    }

    SD_SPI_TransmitBuffer(buffer, SD_SECTOR_SIZE);

    /*
     * Gửi hai byte CRC giả.
     */
    SD_SPI_Transfer(0xFFU);
    SD_SPI_Transfer(0xFFU);

    response = SD_SPI_Transfer(0xFFU);

    /*
     * Năm bit thấp bằng 0x05 nghĩa là dữ liệu được chấp nhận.
     */
    if ((response & 0x1FU) != SD_DATA_ACCEPTED)
    {
        return false;
    }

    return SD_WaitReady(SD_WRITE_TIMEOUT_MS);
}

/* ============================================================
 * Đọc thanh ghi CSD để tính số sector
 * ============================================================ */

static bool SD_ReadCSD(uint8_t csd[16])
{
    uint8_t response;

    response = SD_SendCommand(SD_CMD9, 0U);

    if (response != 0x00U)
    {
        SD_Deselect();
        return false;
    }

    bool result = SD_ReceiveDataBlock(csd, 16U);

    SD_Deselect();

    return result;
}

static uint32_t SD_ParseSectorCount(
    const uint8_t csd[16]
)
{
    /*
     * CSD version 2.0:
     * dùng cho SDHC/SDXC.
     */
    if ((csd[0] & 0xC0U) == 0x40U)
    {
        uint32_t c_size;

        c_size =
            ((uint32_t)(csd[7] & 0x3FU) << 16) |
            ((uint32_t)csd[8] << 8) |
            (uint32_t)csd[9];

        /*
         * Capacity = (C_SIZE + 1) × 512 KiB
         * Số sector 512 byte = (C_SIZE + 1) × 1024
         */
        return (c_size + 1U) * 1024U;
    }

    /*
     * CSD version 1.0:
     * dùng cho SDSC.
     */
    uint32_t read_bl_len;
    uint32_t c_size;
    uint32_t c_size_mult;
    uint64_t capacity_bytes;

    read_bl_len = csd[5] & 0x0FU;

    c_size =
        ((uint32_t)(csd[6] & 0x03U) << 10) |
        ((uint32_t)csd[7] << 2) |
        ((uint32_t)(csd[8] & 0xC0U) >> 6);

    c_size_mult =
        ((uint32_t)(csd[9] & 0x03U) << 1) |
        ((uint32_t)(csd[10] & 0x80U) >> 7);

    capacity_bytes =
        (uint64_t)(c_size + 1U) *
        (uint64_t)(1UL << (c_size_mult + 2U)) *
        (uint64_t)(1UL << read_bl_len);

    return (uint32_t)(
        capacity_bytes / SD_SECTOR_SIZE
    );
}

/* ============================================================
 * Khởi tạo thẻ SD
 * ============================================================ */

bool SD_Init(void)
{
    uint8_t response;
    uint8_t ocr[4];
    uint32_t start_tick;

    sd_initialized = false;
    sd_card_type = SD_CARD_UNKNOWN;
    sd_sector_count = 0U;

    SD_CS_High();

    /*
     * Chờ nguồn thẻ ổn định.
     */
    HAL_Delay(10U);

    /*
     * Cấp ít nhất 74 xung clock khi CS ở mức cao.
     * 10 byte tương ứng 80 xung.
     */
    for (uint8_t i = 0; i < 10U; i++)
    {
        SD_SPI_Transfer(0xFFU);
    }

    /*
     * CMD0: đưa thẻ về Idle State.
     */
    start_tick = HAL_GetTick();

    do
    {
        response = SD_SendCommand(SD_CMD0, 0U);

        if (response == 0x01U)
        {
            break;
        }

    } while ((HAL_GetTick() - start_tick) < SD_INIT_TIMEOUT_MS);

    if (response != 0x01U)
    {
        SD_Deselect();
        return false;
    }

    /*
     * CMD8: kiểm tra thẻ chuẩn SD version 2.
     */
    response = SD_SendCommand(
        SD_CMD8,
        0x000001AAUL
    );

    if (response == 0x01U)
    {
        /*
         * Đọc bốn byte phản hồi R7.
         */
        for (uint8_t i = 0; i < 4U; i++)
        {
            ocr[i] = SD_SPI_Transfer(0xFFU);
        }

        /*
         * Kiểm tra voltage accepted và check pattern.
         */
        if ((ocr[2] != 0x01U) ||
            (ocr[3] != 0xAAU))
        {
            SD_Deselect();
            return false;
        }

        /*
         * Lặp ACMD41 với HCS cho đến khi thẻ sẵn sàng.
         */
        start_tick = HAL_GetTick();

        do
        {
            response = SD_SendCommand(
                (uint8_t)(0x80U | SD_ACMD41),
                0x40000000UL
            );

            if (response == 0x00U)
            {
                break;
            }

        } while ((HAL_GetTick() - start_tick) <
                 SD_INIT_TIMEOUT_MS);

        if (response != 0x00U)
        {
            SD_Deselect();
            return false;
        }

        /*
         * CMD58: đọc OCR để xác định SDHC.
         */
        response = SD_SendCommand(SD_CMD58, 0U);

        if (response != 0x00U)
        {
            SD_Deselect();
            return false;
        }

        for (uint8_t i = 0; i < 4U; i++)
        {
            ocr[i] = SD_SPI_Transfer(0xFFU);
        }

        if ((ocr[0] & 0x40U) != 0U)
        {
            sd_card_type = SD_CARD_SDHC;
        }
        else
        {
            sd_card_type = SD_CARD_SDSC;
        }
    }
    else
    {
        /*
         * Thẻ cũ không hỗ trợ CMD8.
         * Khởi tạo bằng ACMD41 không có HCS.
         */
        start_tick = HAL_GetTick();

        do
        {
            response = SD_SendCommand(
                (uint8_t)(0x80U | SD_ACMD41),
                0U
            );

            if (response == 0x00U)
            {
                break;
            }

        } while ((HAL_GetTick() - start_tick) <
                 SD_INIT_TIMEOUT_MS);

        if (response != 0x00U)
        {
            SD_Deselect();
            return false;
        }

        sd_card_type = SD_CARD_SDSC;
    }

    /*
     * SDSC dùng địa chỉ byte nên cần đặt block dài 512 byte.
     * SDHC luôn sử dụng block 512 byte.
     */
    if (sd_card_type == SD_CARD_SDSC)
    {
        response = SD_SendCommand(
            SD_CMD16,
            SD_SECTOR_SIZE
        );

        if (response != 0x00U)
        {
            SD_Deselect();
            return false;
        }
    }

    SD_Deselect();

    /*
     * Đọc CSD để tính tổng số sector.
     */
    uint8_t csd[16];

    if (!SD_ReadCSD(csd))
    {
        return false;
    }

    sd_sector_count = SD_ParseSectorCount(csd);

    if (sd_sector_count == 0U)
    {
        return false;
    }

    sd_initialized = true;

    return true;
}

/* ============================================================
 * Trạng thái thẻ
 * ============================================================ */

bool SD_IsReady(void)
{
    return sd_initialized;
}

/* ============================================================
 * Chuyển địa chỉ sector
 * ============================================================ */

static uint32_t SD_GetAddress(uint32_t sector)
{
    /*
     * SDSC dùng địa chỉ byte.
     * SDHC dùng địa chỉ block.
     */
    if (sd_card_type == SD_CARD_SDSC)
    {
        return sector * SD_SECTOR_SIZE;
    }

    return sector;
}

/* ============================================================
 * Đọc sector
 * ============================================================ */

bool SD_ReadBlocks(
    uint8_t *buffer,
    uint32_t sector,
    uint32_t count
)
{
    if (!sd_initialized ||
        buffer == NULL ||
        count == 0U)
    {
        return false;
    }

    uint32_t address = SD_GetAddress(sector);

    /*
     * Đọc một sector.
     */
    if (count == 1U)
    {
        uint8_t response;

        response = SD_SendCommand(
            SD_CMD17,
            address
        );

        if (response != 0x00U)
        {
            SD_Deselect();
            return false;
        }

        bool result = SD_ReceiveDataBlock(
            buffer,
            SD_SECTOR_SIZE
        );

        SD_Deselect();

        return result;
    }

    /*
     * Đọc nhiều sector liên tiếp.
     */
    uint8_t response;

    response = SD_SendCommand(
        SD_CMD18,
        address
    );

    if (response != 0x00U)
    {
        SD_Deselect();
        return false;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        if (!SD_ReceiveDataBlock(
                &buffer[i * SD_SECTOR_SIZE],
                SD_SECTOR_SIZE))
        {
            SD_SendCommand(SD_CMD12, 0U);
            SD_Deselect();
            return false;
        }
    }

    /*
     * CMD12 dừng đọc nhiều block.
     */
    SD_SendCommand(SD_CMD12, 0U);
    SD_Deselect();

    return true;
}

/* ============================================================
 * Ghi sector
 * ============================================================ */

bool SD_WriteBlocks(
    const uint8_t *buffer,
    uint32_t sector,
    uint32_t count
)
{
    if (!sd_initialized ||
        buffer == NULL ||
        count == 0U)
    {
        return false;
    }

    uint32_t address = SD_GetAddress(sector);

    /*
     * Ghi một sector.
     */
    if (count == 1U)
    {
        uint8_t response;

        response = SD_SendCommand(
            SD_CMD24,
            address
        );

        if (response != 0x00U)
        {
            SD_Deselect();
            return false;
        }

        bool result = SD_TransmitDataBlock(
            buffer,
            SD_TOKEN_START_BLOCK
        );

        SD_Deselect();

        return result;
    }

    /*
     * Với thẻ SD, ACMD23 có thể thông báo trước
     * số block sắp được ghi.
     */
    SD_SendCommand(
        (uint8_t)(0x80U | SD_CMD23),
        count
    );

    uint8_t response;

    response = SD_SendCommand(
        SD_CMD25,
        address
    );

    if (response != 0x00U)
    {
        SD_Deselect();
        return false;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        if (!SD_TransmitDataBlock(
                &buffer[i * SD_SECTOR_SIZE],
                SD_TOKEN_MULTI_WRITE))
        {
            SD_Deselect();
            return false;
        }
    }

    /*
     * Gửi token kết thúc multi-block write.
     */
    if (!SD_TransmitDataBlock(
            NULL,
            SD_TOKEN_STOP_MULTI_WRITE))
    {
        SD_Deselect();
        return false;
    }

    bool ready = SD_WaitReady(
        SD_WRITE_TIMEOUT_MS
    );

    SD_Deselect();

    return ready;
}

/* ============================================================
 * Thông tin thẻ
 * ============================================================ */

uint32_t SD_GetSectorCount(void)
{
    return sd_sector_count;
}

SD_CardType SD_GetCardType(void)
{
    return sd_card_type;
}
