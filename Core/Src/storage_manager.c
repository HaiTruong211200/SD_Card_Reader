#include "storage_manager.h"

#include <string.h>
#include <stdio.h>

/*
 * Mount filesystem của thẻ SD.
 * Phải gọi hàm này trước khi dùng f_open(), f_read(), f_write(), ...
 */
FRESULT Storage_Mount(void)
{
    return f_mount(&USERFatFS, USERPath, 1);
}

/*
 * Unmount filesystem.
 * Dùng khi muốn nhả quyền truy cập thẻ, ví dụ trước khi chuyển sang USB Mass Storage.
 */
FRESULT Storage_Unmount(void)
{
    return f_mount(NULL, USERPath, 0);
}

/*
 * Test cơ bản:
 * - Tạo file test.txt
 * - Ghi chuỗi "Hello from STM32!\r\n"
 * - Đóng file
 * - Mở lại file
 * - Đọc dữ liệu
 * - So sánh dữ liệu ghi và đọc
 */
FRESULT Storage_FileTest(void)
{
    FIL file;
    FRESULT result;
    UINT bytes_written = 0;
    UINT bytes_read = 0;

    char write_data[] = "Hello from STM32!\r\n";
    char read_data[64];

    memset(read_data, 0, sizeof(read_data));

    result = f_open(&file, "test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (result != FR_OK)
    {
        return result;
    }

    result = f_write(&file, write_data, strlen(write_data), &bytes_written);
    if (result != FR_OK)
    {
        f_close(&file);
        return result;
    }

    if (bytes_written != strlen(write_data))
    {
        f_close(&file);
        return FR_DISK_ERR;
    }

    result = f_sync(&file);
    if (result != FR_OK)
    {
        f_close(&file);
        return result;
    }

    result = f_close(&file);
    if (result != FR_OK)
    {
        return result;
    }

    result = f_open(&file, "test.txt", FA_READ);
    if (result != FR_OK)
    {
        return result;
    }

    result = f_read(&file, read_data, sizeof(read_data) - 1, &bytes_read);
    if (result != FR_OK)
    {
        f_close(&file);
        return result;
    }

    read_data[bytes_read] = '\0';

    result = f_close(&file);
    if (result != FR_OK)
    {
        return result;
    }

    if (bytes_written != bytes_read)
    {
        return FR_INT_ERR;
    }

    if (memcmp(write_data, read_data, bytes_read) != 0)
    {
        return FR_INT_ERR;
    }

    return FR_OK;
}

/*
 * Ghi nối tiếp vào cuối file test.txt.
 * Dùng để kiểm tra chế độ append của FATFS.
 */
FRESULT Storage_AppendTest(void)
{
    FIL file;
    FRESULT result;
    UINT bytes_written = 0;

    char append_data[] = "Append line from STM32\r\n";

    result = f_open(&file, "test.txt", FA_OPEN_APPEND | FA_WRITE);
    if (result != FR_OK)
    {
        return result;
    }

    result = f_write(&file, append_data, strlen(append_data), &bytes_written);
    if (result != FR_OK)
    {
        f_close(&file);
        return result;
    }

    if (bytes_written != strlen(append_data))
    {
        f_close(&file);
        return FR_DISK_ERR;
    }

    result = f_sync(&file);
    if (result != FR_OK)
    {
        f_close(&file);
        return result;
    }

    result = f_close(&file);
    if (result != FR_OK)
    {
        return result;
    }

    return FR_OK;
}

/*
 * Tạo thư mục.
 * Nếu thư mục đã tồn tại thì vẫn coi là thành công.
 */
FRESULT Storage_CreateDir(const char *path)
{
    FRESULT result;

    if (path == NULL)
    {
        return FR_INVALID_NAME;
    }

    result = f_mkdir(path);

    if (result == FR_EXIST)
    {
        return FR_OK;
    }

    return result;
}

/*
 * Ghi một chuỗi text vào file.
 * Nếu file chưa có thì tạo mới.
 * Nếu file đã có thì ghi đè.
 */
FRESULT Storage_WriteTextFile(const char *path, const char *text)
{
    FIL file;
    FRESULT result;
    UINT bytes_written = 0;
    UINT text_length;

    if (path == NULL || text == NULL)
    {
        return FR_INVALID_OBJECT;
    }

    text_length = strlen(text);

    result = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (result != FR_OK)
    {
        return result;
    }

    result = f_write(&file, text, text_length, &bytes_written);
    if (result != FR_OK)
    {
        f_close(&file);
        return result;
    }

    if (bytes_written != text_length)
    {
        f_close(&file);
        return FR_DISK_ERR;
    }

    result = f_sync(&file);
    if (result != FR_OK)
    {
        f_close(&file);
        return result;
    }

    result = f_close(&file);
    if (result != FR_OK)
    {
        return result;
    }

    return FR_OK;
}

/*
 * Đọc file text vào buffer.
 * buffer sẽ được thêm ký tự '\0' ở cuối để dễ xem trong debug.
 */
FRESULT Storage_ReadTextFile(const char *path, char *buffer, uint32_t buffer_size)
{
    FIL file;
    FRESULT result;
    UINT bytes_read = 0;

    if (path == NULL || buffer == NULL || buffer_size == 0)
    {
        return FR_INVALID_OBJECT;
    }

    memset(buffer, 0, buffer_size);

    result = f_open(&file, path, FA_READ);
    if (result != FR_OK)
    {
        return result;
    }

    result = f_read(&file, buffer, buffer_size - 1, &bytes_read);
    if (result != FR_OK)
    {
        f_close(&file);
        return result;
    }

    buffer[bytes_read] = '\0';

    result = f_close(&file);
    if (result != FR_OK)
    {
        return result;
    }

    return FR_OK;
}

/*
 * Liệt kê file/thư mục trong một đường dẫn.
 * Kết quả được ghi vào output.
 *
 * Ví dụ output:
 * [FILE] test.txt
 * [DIR] LOG
 */
FRESULT Storage_ListDir(const char *path, char *output, uint32_t output_size)
{
    DIR dir;
    FILINFO file_info;
    FRESULT result;
    uint32_t offset = 0;
    int written;

    if (path == NULL || output == NULL || output_size == 0)
    {
        return FR_INVALID_OBJECT;
    }

    memset(output, 0, output_size);

    result = f_opendir(&dir, path);
    if (result != FR_OK)
    {
        return result;
    }

    while (1)
    {
        result = f_readdir(&dir, &file_info);
        if (result != FR_OK)
        {
            break;
        }

        /*
         * fname[0] == '\0' nghĩa là đã đọc hết thư mục.
         */
        if (file_info.fname[0] == '\0')
        {
            break;
        }

        written = snprintf(
            &output[offset],
            output_size - offset,
            "%s%s\r\n",
            (file_info.fattrib & AM_DIR) ? "[DIR] " : "[FILE] ",
            file_info.fname
        );

        if (written < 0)
        {
            result = FR_INT_ERR;
            break;
        }

        offset += (uint32_t)written;

        /*
         * Nếu buffer gần đầy thì dừng để tránh tràn bộ nhớ.
         */
        if (offset >= output_size)
        {
            break;
        }
    }

    f_closedir(&dir);

    return result;
}

/*
 * Xóa file hoặc thư mục rỗng.
 * Nếu file không tồn tại thì vẫn coi là không lỗi.
 */
FRESULT Storage_DeleteFile(const char *path)
{
    FRESULT result;

    if (path == NULL)
    {
        return FR_INVALID_NAME;
    }

    result = f_unlink(path);

    if (result == FR_NO_FILE)
    {
        return FR_OK;
    }

    return result;
}

/*
 * Lấy tổng dung lượng, dung lượng trống và dung lượng đã dùng.
 * Đơn vị: MB.
 */
FRESULT Storage_GetCapacity(StorageCapacityInfo *info)
{
    FATFS *fs;
    DWORD free_clusters;
    DWORD total_sectors;
    DWORD free_sectors;
    FRESULT result;

    if (info == NULL)
    {
        return FR_INVALID_OBJECT;
    }

    result = f_getfree(USERPath, &free_clusters, &fs);
    if (result != FR_OK)
    {
        return result;
    }

    /*
     * FATFS:
     * total_sectors = số cluster dữ liệu * số sector mỗi cluster
     */
    total_sectors = (fs->n_fatent - 2) * fs->csize;
    free_sectors  = free_clusters * fs->csize;

    /*
     * 1 MB = 1024 * 1024 byte
     * 1 sector = 512 byte
     * => 1 MB = 2048 sector
     */
    info->total_mb = total_sectors / 2048;
    info->free_mb  = free_sectors / 2048;

    if (info->total_mb >= info->free_mb)
    {
        info->used_mb = info->total_mb - info->free_mb;
    }
    else
    {
        info->used_mb = 0;
    }

    return FR_OK;
}

FRESULT Storage_GetDisplayInfo(StorageDisplayInfo *info)
{
    FRESULT result;
    StorageCapacityInfo capacity;

    if (info == NULL)
    {
        return FR_INVALID_OBJECT;
    }

    memset(info, 0, sizeof(StorageDisplayInfo));

    result = Storage_GetCapacity(&capacity);

    if (result != FR_OK)
    {
        info->status = STORAGE_STATUS_ERROR;
        info->mode = STORAGE_MODE_LOCAL;
        return result;
    }

    info->total_mb = capacity.total_mb;
    info->free_mb  = capacity.free_mb;
    info->used_mb  = capacity.used_mb;

    if (info->total_mb > 0)
    {
        info->used_percent = (uint8_t)((info->used_mb * 100U) / info->total_mb);
    }
    else
    {
        info->used_percent = 0;
    }

    info->status = STORAGE_STATUS_READY;
    info->mode = STORAGE_MODE_LOCAL;

    return FR_OK;
}
