#if !defined(CONF_PLATFORM_MACOSX)
/*
    unsigned char*
*/
#ifndef ENGINE_CLIENT_AUTOUPDATE_H
#define ENGINE_CLIENT_AUTOUPDATE_H

#include <base/system.h>
#include <engine/autoupdate.h>
#include <string>
#include <list>

class CAutoUpdate : public IAutoUpdate
{
public:
	CAutoUpdate();

	void Reset();

	void CheckUpdates(CMenus *pMenus);
	bool Updated() { return m_Updated; }
	bool NeedResetClient() { return m_NeedResetClient; }
	void ExecuteExit();

private:
	bool m_Updated;
	bool m_NeedUpdate;
	bool m_NeedUpdateBackground;
	bool m_NeedUpdateClient;
	bool m_NeedResetClient;
	std::list<std::string> m_vFiles;

protected:
	bool SelfDelete();
	bool GetFile(const char *pFile, const char *dst);
	bool CanUpdate(const char *pFile);
};

#endif
#endif
