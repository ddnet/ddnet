#ifndef ENGINE_CLIENT_URL_ALLOWLIST_H
#define ENGINE_CLIENT_URL_ALLOWLIST_H

#include <base/url.h>

#include <engine/console.h>

class IConfigManager;

class CUrlAllowList
{
	CUrlDomainName m_aUrlAllowList[URL_ALLOWLIST_MAX_ENTRIES];
	size_t m_UrlAllowListCount = 0;

	static void Con_UrlAllowDomain(IConsole::IResult *pResult, void *pUserData);
	static void Con_UrlDisallowDomain(IConsole::IResult *pResult, void *pUserData);
	static void Con_UrlAllowList(IConsole::IResult *pResult, void *pUserData);

public:
	void InitConsole(IConsole *pConsole);
	void AllowDomain(const char *pDomainName);
	bool IsAllowed(const char *pUrl) const;
	void Save(IConfigManager *pConfigManager) const;
};

#endif // ENGINE_CLIENT_URL_ALLOWLIST_H
