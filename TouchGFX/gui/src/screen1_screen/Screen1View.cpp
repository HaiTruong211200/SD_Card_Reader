#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/Unicode.hpp>
#include <stdint.h>

Screen1View::Screen1View()
    : lastReady(false),
      lastTotalMB(0U),
      lastUsedMB(0U),
      lastFreeMB(0U),
      lastFileCount(0U),
      hasCache(false)
{
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    /*
     * Chiều rộng cố định đủ cho số lớn + " MB".
     * Không resize mỗi lần update → tránh chữ MB nhấp nháy.
     */
    statusValue.setWidthHeight(130, statusValue.getHeight());
    totalValue.setWidthHeight(130, totalValue.getHeight());
    usedValue.setWidthHeight(130, usedValue.getHeight());
    freeValue.setWidthHeight(130, freeValue.getHeight());
    filesValue.setWidthHeight(80, filesValue.getHeight());
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::updateStorageInfo(const StorageInfo& info)
{
    uint32_t totalMB =
        (uint32_t)(info.total_bytes / (1024ULL * 1024ULL));

    uint32_t usedMB =
        (uint32_t)(info.used_bytes / (1024ULL * 1024ULL));

    uint32_t freeMB =
        (uint32_t)(info.free_bytes / (1024ULL * 1024ULL));

    if (hasCache &&
        (info.ready == lastReady) &&
        (totalMB == lastTotalMB) &&
        (usedMB == lastUsedMB) &&
        (freeMB == lastFreeMB) &&
        (info.file_count == lastFileCount))
    {
        return;
    }

    lastReady = info.ready;
    lastTotalMB = totalMB;
    lastUsedMB = usedMB;
    lastFreeMB = freeMB;
    lastFileCount = info.file_count;
    hasCache = true;

    if (info.ready)
    {
        Unicode::snprintf(
            statusValueBuffer,
            STATUSVALUE_SIZE,
            "READY"
        );
    }
    else
    {
        Unicode::snprintf(
            statusValueBuffer,
            STATUSVALUE_SIZE,
            "NOT READY"
        );
    }

    Unicode::snprintf(
        totalValueBuffer,
        TOTALVALUE_SIZE,
        "%u",
        (unsigned int)totalMB
    );

    Unicode::snprintf(
        usedValueBuffer,
        USEDVALUE_SIZE,
        "%u",
        (unsigned int)usedMB
    );

    Unicode::snprintf(
        freeValueBuffer,
        FREEVALUE_SIZE,
        "%u",
        (unsigned int)freeMB
    );

    Unicode::snprintf(
        filesValueBuffer,
        FILESVALUE_SIZE,
        "%u",
        (unsigned int)info.file_count
    );

    statusValue.invalidate();
    totalValue.invalidate();
    usedValue.invalidate();
    freeValue.invalidate();
    filesValue.invalidate();
}
