/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_FRIENDS_H
#define ENGINE_CLIENT_FRIENDS_H

#include <engine/console.h>
#include <engine/friends.h>

class IConfigManager;

class CFriends : public IFriends
{
	CFriendInfo m_aFriends[MAX_FRIENDS];
	int m_Foes;
	int m_NumFriends;

	static void ConAddFriend(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveFriend(IConsole::IResult *pResult, void *pUserData);
	static void ConFriends(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	CFriends();

	void Init(bool Foes = false) override;

	int NumFriends() const override { return m_NumFriends; }
	const CFriendInfo *GetFriend(int Index) const override;
	int GetFriendState(const char *pName, const char *pClan) const override;
	bool IsFriend(const char *pName, const char *pClan, bool PlayersOnly) const override;

	void AddFriend(const char *pName, const char *pClan) override;
	void RemoveFriend(const char *pName, const char *pClan) override;
	void RemoveFriend(int Index);
	void Friends();
};

#endif
