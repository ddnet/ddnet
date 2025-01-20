/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/shared/config.h>

#include "friends.h"

CFriends::CFriends()
{
	mem_zero(m_aFriends, sizeof(m_aFriends));
	m_NumFriends = 0;
	m_Type = FRIENDS;
}

void CFriends::ConAddFriend(IConsole::IResult *pResult, void *pUserData)
{
	CFriends *pSelf = (CFriends *)pUserData;
	pSelf->AddFriend(pResult->GetString(0), pResult->GetString(1));
}

void CFriends::ConRemoveFriend(IConsole::IResult *pResult, void *pUserData)
{
	CFriends *pSelf = (CFriends *)pUserData;
	pSelf->RemoveFriend(pResult->GetString(0), pResult->GetString(1));
}

void CFriends::ConFriends(IConsole::IResult *pResult, void *pUserData)
{
	CFriends *pSelf = (CFriends *)pUserData;
	pSelf->Friends();
}

void CFriends::Init(EFriendType Type)
{
	m_Type = Type;

	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this);

	IConsole *pConsole = Kernel()->RequestInterface<IConsole>();
	if(pConsole)
	{
		if(Type == FOES)
		{
			pConsole->Register("add_foe", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddFriend, this, "Add a foe");
			pConsole->Register("remove_foe", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveFriend, this, "Remove a foe");
			pConsole->Register("foes", "", CFGFLAG_CLIENT, ConFriends, this, "List foes");
		}
		else if(Type == HIDDEN)
		{
			pConsole->Register("add_hidden", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddFriend, this, "Add a hidden");
			pConsole->Register("remove_hidden", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveFriend, this, "Remove a hidden");
			pConsole->Register("hidden", "", CFGFLAG_CLIENT, ConFriends, this, "List hidden");
		}
		else
		{
			pConsole->Register("add_friend", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddFriend, this, "Add a friend");
			pConsole->Register("remove_friend", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveFriend, this, "Remove a friend");
			pConsole->Register("friends", "", CFGFLAG_CLIENT, ConFriends, this, "List friends");
		}
	}
}

const CFriendInfo *CFriends::GetFriend(int Index) const
{
	return &m_aFriends[maximum(0, Index % m_NumFriends)];
}

int CFriends::GetFriendState(const char *pName, const char *pClan) const
{
	int Result = FRIEND_NO;
	unsigned NameHash = str_quickhash(pName);
	unsigned ClanHash = str_quickhash(pClan);
	for(int i = 0; i < m_NumFriends; ++i)
	{
		if((g_Config.m_ClFriendsIgnoreClan && m_aFriends[i].m_aName[0]) || (m_aFriends[i].m_ClanHash == ClanHash && !str_comp(m_aFriends[i].m_aClan, pClan)))
		{
			if(m_aFriends[i].m_aName[0] == 0)
				Result = FRIEND_CLAN;
			else if(m_aFriends[i].m_NameHash == NameHash && !str_comp(m_aFriends[i].m_aName, pName))
			{
				Result = FRIEND_PLAYER;
				break;
			}
		}
	}
	return Result;
}

bool CFriends::IsFriend(const char *pName, const char *pClan, bool PlayersOnly) const
{
	unsigned NameHash = str_quickhash(pName);
	unsigned ClanHash = str_quickhash(pClan);
	for(int i = 0; i < m_NumFriends; ++i)
	{
		if(((g_Config.m_ClFriendsIgnoreClan && m_aFriends[i].m_aName[0]) || (m_aFriends[i].m_ClanHash == ClanHash && !str_comp(m_aFriends[i].m_aClan, pClan))) &&
			((!PlayersOnly && m_aFriends[i].m_aName[0] == 0) || (m_aFriends[i].m_NameHash == NameHash && !str_comp(m_aFriends[i].m_aName, pName))))
			return true;
	}
	return false;
}

void CFriends::AddFriend(const char *pName, const char *pClan)
{
	if(m_NumFriends == MAX_FRIENDS || (pName[0] == 0 && pClan[0] == 0))
		return;

	// make sure we don't have the friend already
	unsigned NameHash = str_quickhash(pName);
	unsigned ClanHash = str_quickhash(pClan);
	for(int i = 0; i < m_NumFriends; ++i)
	{
		if((m_aFriends[i].m_NameHash == NameHash && !str_comp(m_aFriends[i].m_aName, pName)) && ((g_Config.m_ClFriendsIgnoreClan && m_aFriends[i].m_aName[0]) || (m_aFriends[i].m_ClanHash == ClanHash && !str_comp(m_aFriends[i].m_aClan, pClan))))
			return;
	}

	str_copy(m_aFriends[m_NumFriends].m_aName, pName);
	str_copy(m_aFriends[m_NumFriends].m_aClan, pClan);
	m_aFriends[m_NumFriends].m_NameHash = NameHash;
	m_aFriends[m_NumFriends].m_ClanHash = ClanHash;
	++m_NumFriends;
}

void CFriends::RemoveFriend(const char *pName, const char *pClan)
{
	unsigned NameHash = str_quickhash(pName);
	unsigned ClanHash = str_quickhash(pClan);
	for(int i = 0; i < m_NumFriends; ++i)
	{
		if((m_aFriends[i].m_NameHash == NameHash && !str_comp(m_aFriends[i].m_aName, pName)) &&
			((g_Config.m_ClFriendsIgnoreClan && m_aFriends[i].m_aName[0]) || (m_aFriends[i].m_ClanHash == ClanHash && !str_comp(m_aFriends[i].m_aClan, pClan))))
		{
			RemoveFriend(i);
			return;
		}
	}
}

void CFriends::RemoveFriend(int Index)
{
	if(Index >= 0 && Index < m_NumFriends)
	{
		mem_move(&m_aFriends[Index], &m_aFriends[Index + 1], sizeof(CFriendInfo) * (m_NumFriends - (Index + 1)));
		--m_NumFriends;
	}
}

void CFriends::Friends()
{
	char aBuf[128];
	IConsole *pConsole = Kernel()->RequestInterface<IConsole>();
	if(pConsole)
	{
		for(int i = 0; i < m_NumFriends; ++i)
		{
			str_format(aBuf, sizeof(aBuf), "Name: %s, Clan: %s", m_aFriends[i].m_aName, m_aFriends[i].m_aClan);

			pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_Type == FOES ? "foes" : m_Type == FRIENDS ? "friends" : "hidden", aBuf, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor)));
		}
	}
}

void CFriends::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CFriends *pSelf = (CFriends *)pUserData;
	char aBuf[128];
	const char *pEnd = aBuf + sizeof(aBuf) - 4;
	for(int i = 0; i < pSelf->m_NumFriends; ++i)
	{
		str_copy(aBuf, pSelf->m_Type == FOES ? "add_foe " : pSelf->m_Type == FRIENDS ? "add_friend " : "add_hidden ");

		str_append(aBuf, "\"");
		char *pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, pSelf->m_aFriends[i].m_aName, pEnd);
		str_append(aBuf, "\" \"");
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, pSelf->m_aFriends[i].m_aClan, pEnd);
		str_append(aBuf, "\"");

		pConfigManager->WriteLine(aBuf);
	}
}
