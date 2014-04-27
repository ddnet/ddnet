/*
    unsigned char*
*/
#ifndef ENGINE_AUTOUPDATE_H
#define ENGINE_AUTOUPDATE_H

#include "kernel.h"
#include <game/client/components/menus.h>
#include <string>

class IAutoUpdate : public IInterface
{
	MACRO_INTERFACE("autoupdate", 0)
public:
	virtual void CheckUpdates(CMenus *pMenus) = 0;
	virtual bool Updated() = 0;
	virtual bool NeedResetClient() = 0;
	virtual void ExecuteExit() = 0;
};

#endif
