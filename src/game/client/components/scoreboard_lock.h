#ifndef GAME_CLIENT_COMPONENTS_SCOREBOARD_LOCK_H
#define GAME_CLIENT_COMPONENTS_SCOREBOARD_LOCK_H

#include <game/client/gameclient.h>

#include <optional>

class CScoreboardLock
{
public:
	CScoreboardLock();

	void Reset();
	void Lock() { m_IsSet = true; }
	bool IsSet() const { return m_IsSet; }
	void Set(const CNetObj_PlayerInfo *const *ppPlayerInfoSorted, const CGameClient::CClientData *pClientData, int Team);
	void Update(const CNetObj_PlayerInfo *const *pPlayerInfoById);

	const CNetObj_PlayerInfo *DisconnectedPlayerInfo(int ArrayId);
	const CGameClient::CClientData *DisconnectedClientData(int ArrayId);
	bool IsDisconnected(int ArrayId) const;
	bool IsInactive(int ArrayId) const;
	int ClientId(int ArrayId) const { return m_aLockedPlayers[ArrayId].m_ClientId; }

private:
	void ResetLocks();

	enum class EPlayerLockState
	{
		CONNECTED,
		DISCONNECTED,
		SLOT_EMPTY,
	};

	class CPlayerLockData
	{
	public:
		int m_ClientId;
		int m_Team;
		int m_Country;
		EPlayerLockState m_Lock;
		char m_aName[16];
		char m_aClan[12];
	};

	CPlayerLockData m_aLockedPlayers[MAX_CLIENTS];
	bool m_IsSet;

	CNetObj_PlayerInfo m_DisconnectedPlayerInfo;
	CGameClient::CClientData m_DisconnectedClientData;
};

#endif
