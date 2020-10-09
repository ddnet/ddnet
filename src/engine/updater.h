#ifndef ENGINE_UPDATER_H
#define ENGINE_UPDATER_H

#include <map>
#include<string>

#include "kernel.h"

class IUpdater : public IInterface
{
	MACRO_INTERFACE("updater", 0)
public:
	enum
	{
		CLEAN = 0,
		GETTING_MANIFEST,
		GOT_MANIFEST,
		DOWNLOADING,
		MOVE_FILES,
		NEED_RESTART,
		FAIL,
	};

	virtual void Update() = 0;
	virtual void InitiateUpdate() = 0;
	virtual void PerformUpdate(const std::map<std::string, bool> &Jobs, bool PreventRestart = false) = 0;

	virtual int State() const = 0;
	virtual float Progress() const = 0;
	virtual char *Speed(char *pBuf, int BufSize) const = 0;
};

#endif
