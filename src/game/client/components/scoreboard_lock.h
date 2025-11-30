#ifndef GAME_CLIENT_COMPONENTS_SCOREBOARD_LOCK_H
#define GAME_CLIENT_COMPONENTS_SCOREBOARD_LOCK_H

#include <game/client/gameclient.h>

#include <optional>

class CScoreboardLock
{
public:
	CScoreboardLock();

	void Reset();
	bool IsSet() const { return m_IsSet; }
	void Set(const CNetObj_GameInfo *pGameInfo, const CNetObj_GameData *pGameData, const CTeamsCore *pTeamCore,
		const CGameClient::CClientData *pClientData, const CNetObj_PlayerInfo *const *ppPlayerInfo, const CTranslationContext::CClientData *pClientTranslationData);

	const CNetObj_GameInfo *GameInfo() { return m_LockGameInfoObj.has_value() ? &m_LockGameInfoObj.value() : nullptr; }
	const CNetObj_GameData *GameData() { return m_LockGameDataObj.has_value() ? &m_LockGameDataObj.value() : nullptr; }
	const CTeamsCore *TeamsCore() { return m_LockTeamCore.has_value() ? &m_LockTeamCore.value() : nullptr; }

	const CGameClient::CClientData *ClientDataList() { return m_aClients; }
	const CTranslationContext::CClientData *TranslationDataList() { return m_aTranslationData; }

	const CNetObj_PlayerInfo *PlayerInfo(int ClientId, const CNetObj_PlayerInfo *const *Original);

private:
	bool m_IsSet;
	std::optional<CNetObj_GameInfo> m_LockGameInfoObj;
	std::optional<CNetObj_GameData> m_LockGameDataObj;
	std::optional<CTeamsCore> m_LockTeamCore;

	CGameClient::CClientData m_aClients[MAX_CLIENTS];
	CNetObj_PlayerInfo m_aPlayerInfos[MAX_CLIENTS];
	CTranslationContext::CClientData m_aTranslationData[MAX_CLIENTS];
};

#endif
