#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/Unicode.hpp>
#include <stdint.h>

Screen1View::Screen1View()
{
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::updateStorageInfo(const StorageInfo& info)
{
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

    uint32_t totalMB =
        (uint32_t)(info.total_bytes / (1024ULL * 1024ULL));

    uint32_t usedMB =
        (uint32_t)(info.used_bytes / (1024ULL * 1024ULL));

    uint32_t freeMB =
        (uint32_t)(info.free_bytes / (1024ULL * 1024ULL));

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
		(unsigned int) freeMB
    );

    statusValue.invalidate();
    totalValue.invalidate();
    usedValue.invalidate();
    freeValue.invalidate();
}
