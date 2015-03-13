#ifndef ENGINE_AUTOUPDATE_H
#define ENGINE_AUTOUPDATE_H

#include "kernel.h"

class IAutoUpdate : public IInterface
{
    MACRO_INTERFACE("autoupdate", 0)
public:
    enum
    {
        CLEAN = 0,
        GETTING_MANIFEST,
        GOT_MANIFEST,
        PARSING_UPDATE,
        DOWNLOADING,
        NEED_RESTART,
        FAIL,
    };

    virtual void Update() = 0;
    virtual void InitiateUpdate() = 0;

    virtual int GetCurrentState() = 0;
    virtual char *GetCurrentFile() = 0;
    virtual int GetCurrentPercent() = 0;
};

#endif
