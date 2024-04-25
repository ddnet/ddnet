#include "name_ban.h"

#include <base/system.h>

#include <engine/shared/config.h>

CNameBan::CNameBan(const char *pName, const char *pReason, int Distance, bool IsSubstring) :
	m_Distance(Distance), m_IsSubstring(IsSubstring)
{
	str_copy(m_aName, pName);
	str_copy(m_aReason, pReason);
	m_SkeletonLength = str_utf8_to_skeleton(m_aName, m_aSkeleton, std::size(m_aSkeleton));
}

void CNameBans::InitConsole(IConsole *pConsole)
{
	m_pConsole = pConsole;

	m_pConsole->Register("name_ban", "s[name] ?i[distance] ?i[is_substring] ?r[reason]", CFGFLAG_SERVER, ConNameBan, this, "Ban a certain nickname");
	m_pConsole->Register("name_unban", "s[name]", CFGFLAG_SERVER, ConNameUnban, this, "Unban a certain nickname");
	m_pConsole->Register("name_bans", "", CFGFLAG_SERVER, ConNameBans, this, "List all name bans");
}

void CNameBans::Ban(const char *pName, const char *pReason, const int Distance, const bool IsSubstring)
{
	for(auto &Ban : m_vNameBans)
	{
		if(str_comp(Ban.m_aName, pName) == 0)
		{
			if(m_pConsole)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "changed name='%s' distance=%d old_distance=%d is_substring=%d old_is_substring=%d reason='%s' old_reason='%s'", pName, Distance, Ban.m_Distance, IsSubstring, Ban.m_IsSubstring, pReason, Ban.m_aReason);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name_ban", aBuf);
			}
			str_copy(Ban.m_aReason, pReason);
			Ban.m_Distance = Distance;
			Ban.m_IsSubstring = IsSubstring;
			return;
		}
	}

	m_vNameBans.emplace_back(pName, pReason, Distance, IsSubstring);
	if(m_pConsole)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "added name='%s' distance=%d is_substring=%d reason='%s'", pName, Distance, IsSubstring, pReason);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name_ban", aBuf);
	}
}

void CNameBans::Unban(const char *pName)
{
	auto ToRemove = std::remove_if(m_vNameBans.begin(), m_vNameBans.end(), [pName](const CNameBan &Ban) { return str_comp(Ban.m_aName, pName) == 0; });
	if(ToRemove == m_vNameBans.end())
	{
		if(m_pConsole)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "name ban '%s' not found", pName);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name_ban", aBuf);
		}
	}
	else
	{
		if(m_pConsole)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "removed name='%s' distance=%d is_substring=%d reason='%s'", (*ToRemove).m_aName, (*ToRemove).m_Distance, (*ToRemove).m_IsSubstring, (*ToRemove).m_aReason);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name_ban", aBuf);
		}
		m_vNameBans.erase(ToRemove, m_vNameBans.end());
	}
}

void CNameBans::Dump() const
{
	if(!m_pConsole)
		return;

	char aBuf[256];
	for(const auto &Ban : m_vNameBans)
	{
		str_format(aBuf, sizeof(aBuf), "name='%s' distance=%d is_substring=%d reason='%s'", Ban.m_aName, Ban.m_Distance, Ban.m_IsSubstring, Ban.m_aReason);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "name_ban", aBuf);
	}
}

const CNameBan *CNameBans::IsBanned(const char *pName) const
{
	char aTrimmed[MAX_NAME_LENGTH];
	str_copy(aTrimmed, str_utf8_skip_whitespaces(pName));
	str_utf8_trim_right(aTrimmed);

	int aSkeleton[MAX_NAME_SKELETON_LENGTH];
	int SkeletonLength = str_utf8_to_skeleton(aTrimmed, aSkeleton, std::size(aSkeleton));
	int aBuffer[MAX_NAME_SKELETON_LENGTH * 2 + 2];

	const CNameBan *pResult = nullptr;
	for(const CNameBan &Ban : m_vNameBans)
	{
		int Distance = str_utf32_dist_buffer(aSkeleton, SkeletonLength, Ban.m_aSkeleton, Ban.m_SkeletonLength, aBuffer, std::size(aBuffer));
		if(Distance <= Ban.m_Distance || (Ban.m_IsSubstring && str_utf8_find_nocase(pName, Ban.m_aName)))
			pResult = &Ban;
	}
	return pResult;
}

void CNameBans::ConNameBan(IConsole::IResult *pResult, void *pUser)
{
	const char *pName = pResult->GetString(0);
	const char *pReason = pResult->NumArguments() > 3 ? pResult->GetString(3) : "";
	const int Distance = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : str_length(pName) / 3;
	const bool IsSubstring = pResult->NumArguments() > 2 ? pResult->GetInteger(2) != 0 : false;
	static_cast<CNameBans *>(pUser)->Ban(pName, pReason, Distance, IsSubstring);
}

void CNameBans::ConNameUnban(IConsole::IResult *pResult, void *pUser)
{
	const char *pName = pResult->GetString(0);
	static_cast<CNameBans *>(pUser)->Unban(pName);
}

void CNameBans::ConNameBans(IConsole::IResult *pResult, void *pUser)
{
	static_cast<CNameBans *>(pUser)->Dump();
}
