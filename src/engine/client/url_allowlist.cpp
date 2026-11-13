#include "url_allowlist.h"

#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/config.h>
#include <engine/shared/config.h>

void CUrlAllowList::InitConsole(IConsole *pConsole)
{
	pConsole->Register("url_allow_domain", "?s[domain]", CFGFLAG_CLIENT, Con_UrlAllowDomain, this, "Add a domain to trusted domains that can be safely opened");
	pConsole->Register("url_disallow_domain", "?s[domain]", CFGFLAG_CLIENT, Con_UrlDisallowDomain, this, "Remove a domain from trusted domains that can be safely opened");
	pConsole->Register("url_allow_list", "", CFGFLAG_CLIENT, Con_UrlAllowList, this, "List trusted domains that can be safely opened");
}

void CUrlAllowList::Con_UrlAllowDomain(IConsole::IResult *pResult, void *pUserData)
{
	CUrlAllowList *pSelf = (CUrlAllowList *)pUserData;
	const char *pDomainName = pResult->GetString(0);
	pSelf->AllowDomain(pDomainName);
}

void CUrlAllowList::Con_UrlDisallowDomain(IConsole::IResult *pResult, void *pUserData)
{
	CUrlAllowList *pSelf = (CUrlAllowList *)pUserData;
	const char *pDomainName = pResult->GetString(0);

	int Index = -1;
	for(size_t i = 0; i < pSelf->m_UrlAllowListCount && Index == -1; ++i)
	{
		if(str_utf8_comp_nocase(pSelf->m_aUrlAllowList[i].m_aName, pDomainName) == 0)
			Index = (int)i;
	}

	if(Index == -1)
	{
		log_warn("url", "This domain name is already excluded");
		return;
	}

	mem_move(&pSelf->m_aUrlAllowList[Index], &pSelf->m_aUrlAllowList[Index + 1], sizeof(CUrlDomainName) * (pSelf->m_UrlAllowListCount - (Index + 1)));
	--pSelf->m_UrlAllowListCount;
}

void CUrlAllowList::Con_UrlAllowList(IConsole::IResult *pResult, void *pUserData)
{
	CUrlAllowList *pSelf = (CUrlAllowList *)pUserData;
	for(size_t i = 0; i < pSelf->m_UrlAllowListCount; ++i)
	{
		log_info("url", "%s", pSelf->m_aUrlAllowList[i].m_aName);
	}
}

void CUrlAllowList::AllowDomain(const char *pDomainName)
{
	if(m_UrlAllowListCount >= URL_ALLOWLIST_MAX_ENTRIES)
	{
		log_error("url", "Error: maximum number of allowed domains reached");
		return;
	}

	if(!pDomainName)
	{
		log_error("url", "Domain name cannot be empty");
		return;
	}

	for(size_t i = 0; i < m_UrlAllowListCount; ++i)
	{
		if(str_utf8_comp_nocase(m_aUrlAllowList[i].m_aName, pDomainName) == 0)
			return; // don't add duplicates
	}

	const size_t DomainNameLength = str_length(pDomainName);
	if(DomainNameLength >= URL_MAX_LENGTH || !url_is_valid_domain_name(pDomainName, DomainNameLength))
	{
		log_error("url", "Invalid domain name");
		return;
	}

	str_copy(m_aUrlAllowList[m_UrlAllowListCount].m_aName, pDomainName);
	m_aUrlAllowList[m_UrlAllowListCount].m_Length = DomainNameLength;

	m_UrlAllowListCount++;
}

bool CUrlAllowList::IsAllowed(const char *pUrl) const
{
	for(size_t i = 0; i < m_UrlAllowListCount; ++i)
	{
		if(url_check_domain_name(pUrl, m_aUrlAllowList[i].m_aName))
			return true;
	}

	return false;
}

void CUrlAllowList::Save(IConfigManager *pConfigManager) const
{
	char aBuf[URL_MAX_LENGTH + 32];
	const char *pEnd = aBuf + sizeof(aBuf) - 4;
	for(size_t i = 0; i < m_UrlAllowListCount; ++i)
	{
		str_copy(aBuf, "url_allow_domain ");

		str_append(aBuf, "\"");
		char *pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, m_aUrlAllowList[i].m_aName, pEnd);
		str_append(aBuf, "\"");

		pConfigManager->WriteLine(aBuf);
	}
}
