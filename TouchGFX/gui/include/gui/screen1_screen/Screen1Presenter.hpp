#ifndef SCREEN1PRESENTER_HPP
#define SCREEN1PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Screen1View;

class Screen1Presenter : public touchgfx::Presenter,
                         public ModelListener
{
public:
    Screen1Presenter(Screen1View& v);

    virtual void activate();
    virtual void deactivate();

    virtual void storageInfoChanged(
        const StorageInfo& info
    );

private:
    Screen1Presenter();

    Screen1View& view;
};

#endif
