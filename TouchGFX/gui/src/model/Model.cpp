#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>

Model::Model()
    : modelListener(0),
      tickCounter(0)
{
}

void Model::tick()
{
    tickCounter++;

    if (tickCounter >= 60U)
    {
        tickCounter = 0U;

        StorageInfo info = {};

        if (Storage_TryLock(10) != 0)
        {
            (void)Storage_ReadInfo(&info);
            Storage_Unlock();

            if (modelListener != 0)
            {
                modelListener->storageInfoChanged(info);
            }
        }
    }
}
