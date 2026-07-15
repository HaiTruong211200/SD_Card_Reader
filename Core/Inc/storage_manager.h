#ifndef INC_STORAGE_MANAGER_H_
#define INC_STORAGE_MANAGER_H_

#include "fatfs.h"
#include <stdbool.h>

bool Storage_Mount(void);
bool Storage_CreateAndWriteTest(void);
bool Storage_ReadTest(void);
bool Storage_CompareTest(void);

bool Storage_AppendTest(void);
bool Storage_ReadAfterAppendTest(void);
bool Storage_CompareAfterAppendTest(void);
bool Storage_DeleteTest(void);

bool Storage_GetInfo(void);

extern uint64_t storage_total_bytes;
extern uint64_t storage_free_bytes;
extern uint64_t storage_used_bytes;

extern FRESULT storage_fr;
extern UINT storage_bytes_written;
extern UINT storage_bytes_read;
extern char storage_read_data[64];

static const char storage_append_data[] =
    "Appended line\r\n";

static const char storage_expected_after_append[] =
    "Hello from STM32!\r\n"
    "Appended line\r\n";

#define STORAGE_MAX_FILES 20
#define STORAGE_NAME_LENGTH 64

bool Storage_ListRoot(void);

extern uint32_t storage_file_count;
extern char storage_file_names[STORAGE_MAX_FILES][STORAGE_NAME_LENGTH];

typedef struct
{
    bool ready;
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    uint32_t file_count;
} StorageInfo;

bool Storage_ReadInfo(StorageInfo *info);

/* Generic file API — dùng cho Work 4 (UART PC protocol) */
bool Storage_WriteFile(const char *path, const void *data, UINT len, UINT *written);
bool Storage_AppendFile(const char *path, const void *data, UINT len, UINT *written);
bool Storage_ReadFile(const char *path, void *buf, UINT buflen, UINT *read);
bool Storage_DeleteFile(const char *path);
bool Storage_FileExists(const char *path);

/* Mutex chia sẻ FatFs giữa GUI và UART (timeout_ms = 0 → try) */
int Storage_TryLock(uint32_t timeout_ms);
void Storage_Unlock(void);

#endif
