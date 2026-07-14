#ifndef MODELLISTENER_HPP
#define MODELLISTENER_HPP

#include <gui/model/Model.hpp>

class ModelListener
{
public:
    ModelListener() : model(0)
    {
    }

    virtual ~ModelListener()
    {
    }

    virtual void storageInfoChanged(const StorageInfo& info)
    {
    }

    void bind(Model* m)
    {
        model = m;
    }

protected:
    Model* model;
};

#endif
