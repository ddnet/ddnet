#ifndef ENGINE_AUTOUPDATE_H
#define ENGINE_AUTOUPDATE_H

#include "kernel.h"

class IAutoUpdate : public IInterface
{
    MACRO_INTERFACE("autoupdate", 0)
public:
    enum
    {
        IGNORED = -1,
        CLEAN,
        GETTING_MANIFEST,
        GOT_MANIFEST,
        PARSING_UPDATE,
        DOWNLOADING,
        NEED_RESTART,
        FAIL_MANIFEST,
    };

    virtual void Update() = 0;
    virtual void InitiateUpdate() = 0;
    virtual void IgnoreUpdate() = 0;

    virtual int GetCurrentState() = 0;
    virtual char *GetCurrentFile() = 0;
    virtual int GetCurrentPercent() = 0;
};

#endif
