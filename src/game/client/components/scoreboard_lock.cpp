#include "scoreboard_lock.h"

CScoreboardLock::CScoreboardLock()
{
	m_IsSet = false;
}

void CScoreboardLock::Reset()
{
	if(m_IsSet)
	{
		m_IsSet = false;
		m_LockGameInfoObj = std::nullopt;
		m_LockGameDataObj = std::nullopt;
		m_LockTeamCore = std::nullopt;
	}
}

void CScoreboardLock::Set(const CNetObj_GameInfo *pGameInfo, const CNetObj_GameData *pGameData, const CTeamsCore *pTeamCore,
	const CGameClient::CClientData *pClientData, const CNetObj_PlayerInfo *const *ppPlayerInfo, const CTranslationContext::CClientData *pClientTranslationData)
{
	m_IsSet = true;
	if(pGameInfo)
		m_LockGameInfoObj = *pGameInfo;

	//m_LockGameDataObj = *pGameData;
	if(pTeamCore)
		m_LockTeamCore = *pTeamCore;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(pClientData)
			m_aClients[ClientId] = pClientData[ClientId];

		if(pClientTranslationData)
			m_aTranslationData[ClientId] = pClientTranslationData[ClientId];

		if(ppPlayerInfo && ppPlayerInfo[ClientId])
		{
			m_aPlayerInfos[ClientId] = *ppPlayerInfo[ClientId];
		}
		else
		{
			m_aPlayerInfos[ClientId].m_Team = -2; // identify empty by impossible team
		}
	}
}

const CNetObj_PlayerInfo *CScoreboardLock::PlayerInfo(int ClientId, const CNetObj_PlayerInfo *const *Original)
{
	if(!m_IsSet)
		return Original[ClientId];
	return m_aPlayerInfos[ClientId].m_Team == -2 ? nullptr : &m_aPlayerInfos[ClientId];
}
