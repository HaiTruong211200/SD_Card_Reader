#include "storage_manager.h"
#include "sd_spi.h"

#include <string.h>

FRESULT storage_fr;

UINT storage_bytes_written = 0;
UINT storage_bytes_read = 0;

uint64_t storage_total_bytes = 0;
uint64_t storage_free_bytes = 0;
uint64_t storage_used_bytes = 0;

char storage_read_data[64] = {0};

static const char storage_write_data[] =
    "Hello from STM32!\r\n";

uint32_t storage_file_count = 0;

char storage_file_names
    [STORAGE_MAX_FILES]
    [STORAGE_NAME_LENGTH];

bool Storage_Mount(void)
{
    storage_fr = f_mount(
        &USERFatFS,
        USERPath,
        1
    );

    return (storage_fr == FR_OK);
}

bool Storage_CreateAndWriteTest(void)
{
    FIL file;

    storage_bytes_written = 0;

    storage_fr = f_open(
        &file,
        "test.txt",
        FA_CREATE_ALWAYS | FA_WRITE
    );

    if (storage_fr != FR_OK)
    {
        return false;
    }

    storage_fr = f_write(
        &file,
        storage_write_data,
        strlen(storage_write_data),
        &storage_bytes_written
    );

    if (storage_fr != FR_OK)
    {
        f_close(&file);
        return false;
    }

    storage_fr = f_close(&file);

    if (storage_fr != FR_OK)
    {
        return false;
    }

    return (
        storage_bytes_written ==
        strlen(storage_write_data)
    );
}

bool Storage_ReadTest(void)
{
    FIL file;

    storage_bytes_read = 0;

    memset(
        storage_read_data,
        0,
        sizeof(storage_read_data)
    );

    storage_fr = f_open(
        &file,
        "test.txt",
        FA_READ
    );

    if (storage_fr != FR_OK)
    {
        return false;
    }

    storage_fr = f_read(
        &file,
        storage_read_data,
        sizeof(storage_read_data) - 1U,
        &storage_bytes_read
    );

    if (storage_fr != FR_OK)
    {
        f_close(&file);
        return false;
    }

    storage_fr = f_close(&file);

    if (storage_fr != FR_OK)
    {
        return false;
    }

    storage_read_data[storage_bytes_read] = '\0';

    return true;
}

bool Storage_CompareTest(void)
{
    return (
        strcmp(
            storage_write_data,
            storage_read_data
        ) == 0
    );
}

bool Storage_AppendTest(void)
{
    FIL file;

    storage_bytes_written = 0;

    storage_fr = f_open(
        &file,
        "test.txt",
        FA_OPEN_ALWAYS | FA_WRITE
    );

    if (storage_fr != FR_OK)
    {
        return false;
    }

    storage_fr = f_lseek(
        &file,
        f_size(&file)
    );

    if (storage_fr != FR_OK)
    {
        f_close(&file);
        return false;
    }

    storage_fr = f_write(
        &file,
        storage_append_data,
        strlen(storage_append_data),
        &storage_bytes_written
    );

    if (storage_fr != FR_OK)
    {
        f_close(&file);
        return false;
    }

    storage_fr = f_close(&file);

    if (storage_fr != FR_OK)
    {
        return false;
    }

    return (
        storage_bytes_written ==
        strlen(storage_append_data)
    );
}

bool Storage_ReadAfterAppendTest(void)
{
    FIL file;

    storage_bytes_read = 0;

    memset(
        storage_read_data,
        0,
        sizeof(storage_read_data)
    );

    storage_fr = f_open(
        &file,
        "test.txt",
        FA_READ
    );

    if (storage_fr != FR_OK)
    {
        return false;
    }

    storage_fr = f_read(
        &file,
        storage_read_data,
        sizeof(storage_read_data) - 1U,
        &storage_bytes_read
    );

    if (storage_fr != FR_OK)
    {
        f_close(&file);
        return false;
    }

    storage_fr = f_close(&file);

    if (storage_fr != FR_OK)
    {
        return false;
    }

    storage_read_data[storage_bytes_read] = '\0';

    return true;
}

bool Storage_CompareAfterAppendTest(void)
{
    return (
        strcmp(
            storage_expected_after_append,
            storage_read_data
        ) == 0
    );
}


bool Storage_ListRoot(void)
{
    DIR dir;
    FILINFO fno;

    storage_file_count = 0;

    memset(
        storage_file_names,
        0,
        sizeof(storage_file_names)
    );

    storage_fr = f_opendir(
        &dir,
        USERPath
    );

    if (storage_fr != FR_OK)
    {
        return false;
    }

    while (1)
    {
        storage_fr = f_readdir(
            &dir,
            &fno
        );

        if (storage_fr != FR_OK)
        {
            f_closedir(&dir);
            return false;
        }

        /*
         * fno.fname[0] == 0
         * nghĩa là đã đọc hết thư mục.
         */
        if (fno.fname[0] == '\0')
        {
            break;
        }

        if (storage_file_count < STORAGE_MAX_FILES)
        {
            strncpy(
                storage_file_names[storage_file_count],
                fno.fname,
                STORAGE_NAME_LENGTH - 1U
            );

            storage_file_names
                [storage_file_count]
                [STORAGE_NAME_LENGTH - 1U] = '\0';

            storage_file_count++;
        }
    }

    storage_fr = f_closedir(&dir);

    return (storage_fr == FR_OK);
}

bool Storage_DeleteTest(void)
{
    storage_fr = f_unlink("test.txt");

    return (storage_fr == FR_OK);
}

bool Storage_GetInfo(void)
{
    FATFS *fs;
    DWORD free_clusters;

    storage_fr = f_getfree(
        USERPath,
        &free_clusters,
        &fs
    );

    if (storage_fr != FR_OK)
    {
        return false;
    }

    uint64_t total_sectors =
        (uint64_t)(fs->n_fatent - 2U) *
        (uint64_t)fs->csize;

    uint64_t free_sectors =
        (uint64_t)free_clusters *
        (uint64_t)fs->csize;

    storage_total_bytes =
        total_sectors * 512ULL;

    storage_free_bytes =
        free_sectors * 512ULL;

    storage_used_bytes =
        storage_total_bytes -
        storage_free_bytes;

    return true;
}

bool Storage_ReadInfo(StorageInfo *info)
{
    if (info == NULL)
    {
        return false;
    }

    info->file_count = 0U;

    if (!SD_IsReady())
    {
        info->ready = false;
        return false;
    }

    if (!Storage_GetInfo())
    {
        info->ready = false;
        return false;
    }

    info->ready = true;
    info->total_bytes = storage_total_bytes;
    info->used_bytes  = storage_used_bytes;
    info->free_bytes  = storage_free_bytes;

    if (Storage_ListRoot())
    {
        info->file_count = storage_file_count;
    }

    return true;
}

bool Storage_WriteFile(const char *path, const void *data, UINT len, UINT *written)
{
    FIL file;
    UINT bw = 0;

    if ((path == NULL) || (data == NULL))
    {
        return false;
    }

    storage_fr = f_open(
        &file,
        path,
        FA_CREATE_ALWAYS | FA_WRITE
    );

    if (storage_fr != FR_OK)
    {
        return false;
    }

    storage_fr = f_write(&file, data, len, &bw);

    if (storage_fr != FR_OK)
    {
        f_close(&file);
        return false;
    }

    storage_fr = f_close(&file);

    if (written != NULL)
    {
        *written = bw;
    }

    return (storage_fr == FR_OK);
}

bool Storage_AppendFile(const char *path, const void *data, UINT len, UINT *written)
{
    FIL file;
    UINT bw = 0;

    if ((path == NULL) || (data == NULL))
    {
        return false;
    }

    storage_fr = f_open(
        &file,
        path,
        FA_OPEN_ALWAYS | FA_WRITE
    );

    if (storage_fr != FR_OK)
    {
        return false;
    }

    storage_fr = f_lseek(&file, f_size(&file));

    if (storage_fr != FR_OK)
    {
        f_close(&file);
        return false;
    }

    storage_fr = f_write(&file, data, len, &bw);

    if (storage_fr != FR_OK)
    {
        f_close(&file);
        return false;
    }

    storage_fr = f_close(&file);

    if (written != NULL)
    {
        *written = bw;
    }

    return (storage_fr == FR_OK);
}

bool Storage_ReadFile(const char *path, void *buf, UINT buflen, UINT *read)
{
    FIL file;
    UINT br = 0;

    if ((path == NULL) || (buf == NULL) || (buflen == 0U))
    {
        return false;
    }

    storage_fr = f_open(&file, path, FA_READ);

    if (storage_fr != FR_OK)
    {
        return false;
    }

    storage_fr = f_read(&file, buf, buflen, &br);

    if (storage_fr != FR_OK)
    {
        f_close(&file);
        return false;
    }

    storage_fr = f_close(&file);

    if (read != NULL)
    {
        *read = br;
    }

    return (storage_fr == FR_OK);
}

bool Storage_DeleteFile(const char *path)
{
    if (path == NULL)
    {
        return false;
    }

    storage_fr = f_unlink(path);
    return (storage_fr == FR_OK);
}

bool Storage_FileExists(const char *path)
{
    FILINFO fno;

    if (path == NULL)
    {
        return false;
    }

    storage_fr = f_stat(path, &fno);
    return (storage_fr == FR_OK);
}
