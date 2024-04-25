#ifndef ENGINE_SERVER_NAME_BAN_H
#define ENGINE_SERVER_NAME_BAN_H

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <vector>

enum
{
	MAX_NAME_SKELETON_LENGTH = MAX_NAME_LENGTH * 4,
	MAX_NAMEBAN_REASON_LENGTH = 64
};

class CNameBan
{
public:
	CNameBan(const char *pName, const char *pReason, int Distance, bool IsSubstring);

	char m_aName[MAX_NAME_LENGTH];
	char m_aReason[MAX_NAMEBAN_REASON_LENGTH];
	int m_aSkeleton[MAX_NAME_SKELETON_LENGTH];
	int m_SkeletonLength;
	int m_Distance;
	bool m_IsSubstring;
};

class CNameBans
{
	IConsole *m_pConsole = nullptr;
	std::vector<CNameBan> m_vNameBans;

	static void ConNameBan(IConsole::IResult *pResult, void *pUser);
	static void ConNameUnban(IConsole::IResult *pResult, void *pUser);
	static void ConNameBans(IConsole::IResult *pResult, void *pUser);

public:
	void InitConsole(IConsole *pConsole);
	void Ban(const char *pName, const char *pReason, const int Distance, const bool IsSubstring);
	void Unban(const char *pName);
	void Dump() const;
	const CNameBan *IsBanned(const char *pName) const;
};

#endif // ENGINE_SERVER_NAME_BAN_H
