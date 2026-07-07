#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "fatfs.h"
#include <stdint.h>

#define STORAGE_DIR_LIST_SIZE 512U

typedef struct
{
    uint32_t total_mb;
    uint32_t free_mb;
    uint32_t used_mb;
} StorageCapacityInfo;

typedef enum
{
    STORAGE_STATUS_NO_CARD = 0,
    STORAGE_STATUS_READY,
    STORAGE_STATUS_BUSY,
    STORAGE_STATUS_ERROR
} StorageStatus;

typedef enum
{
    STORAGE_MODE_LOCAL = 0,
    STORAGE_MODE_USB,
    STORAGE_MODE_UART
} StorageMode;

typedef struct
{
    uint32_t total_mb;
    uint32_t free_mb;
    uint32_t used_mb;
    uint8_t used_percent;

    StorageStatus status;
    StorageMode mode;
} StorageDisplayInfo;

FRESULT Storage_Mount(void);
FRESULT Storage_Unmount(void);

FRESULT Storage_FileTest(void);
FRESULT Storage_AppendTest(void);

FRESULT Storage_CreateDir(const char *path);
FRESULT Storage_WriteTextFile(const char *path, const char *text);
FRESULT Storage_ReadTextFile(const char *path, char *buffer, uint32_t buffer_size);

FRESULT Storage_ListDir(const char *path, char *output, uint32_t output_size);
FRESULT Storage_DeleteFile(const char *path);

FRESULT Storage_GetCapacity(StorageCapacityInfo *info);

FRESULT Storage_GetDisplayInfo(StorageDisplayInfo *info);



#endif
