#ifndef ENGINE_UPDATER_H
#define ENGINE_UPDATER_H

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

	virtual int GetCurrentState() = 0;
	virtual float GetCurrentProgress() = 0;
	virtual void GetDownloadSpeed(char *pBuf, int BufSize) = 0;
};

#endif
