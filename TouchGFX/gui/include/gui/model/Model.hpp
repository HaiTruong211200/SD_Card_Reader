#ifndef MODEL_HPP
#define MODEL_HPP

#include <stdint.h>

extern "C"
{
#include "storage_manager.h"
}

class ModelListener;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();

protected:
    ModelListener* modelListener;

private:
    uint32_t tickCounter;
};

#endif
