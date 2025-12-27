#include "scoreboard_lock.h"

CScoreboardLock::CScoreboardLock()
{
	m_IsSet = false;

	ResetLocks();

	// these will be overwritten on demand
	m_DisconnectedPlayerInfo.m_ClientId = -1;
	m_DisconnectedPlayerInfo.m_Team = -2;
	m_DisconnectedClientData.m_aClan[0] = '\0';
	m_DisconnectedClientData.m_aName[0] = '\0';
	m_DisconnectedClientData.m_Country = 0;

	// these are fixed
	m_DisconnectedPlayerInfo.m_Latency = 999; // bad ping
	m_DisconnectedPlayerInfo.m_Local = 0;
	m_DisconnectedPlayerInfo.m_Score = -9999; // show no time

	m_DisconnectedClientData.m_Active = true; // we want the disconnected player to show up
	m_DisconnectedClientData.m_aSkinName[0] = '\0';
	m_DisconnectedClientData.m_Team = 0; // might be wrong in teammodes :(
	m_DisconnectedClientData.m_Afk = false;
	m_DisconnectedClientData.m_AuthLevel = 0; // not authed
}

void CScoreboardLock::Reset()
{
	if(m_IsSet)
	{
		m_IsSet = false;
		ResetLocks();
	}
}

void CScoreboardLock::Set(const CNetObj_PlayerInfo *const *ppPlayerInfoSorted, const CGameClient::CClientData *pClientData, int Team)
{
	for(int ArrayId = 0; ArrayId < MAX_CLIENTS; ArrayId++)
	{
		const CNetObj_PlayerInfo *pInfo = ppPlayerInfoSorted[ArrayId];
		if(pInfo && pInfo->m_Team == Team)
		{
			m_aLockedPlayers[ArrayId].m_Team = pInfo->m_Team;
			m_aLockedPlayers[ArrayId].m_ClientId = pInfo->m_ClientId;
			m_aLockedPlayers[ArrayId].m_Lock = EPlayerLockState::CONNECTED;

			m_aLockedPlayers[ArrayId].m_Country = pClientData[pInfo->m_ClientId].m_Country;
			str_copy(m_aLockedPlayers[ArrayId].m_aName, pClientData[pInfo->m_ClientId].m_aName);
			str_copy(m_aLockedPlayers[ArrayId].m_aClan, pClientData[pInfo->m_ClientId].m_aClan);
		}
	}
}

void CScoreboardLock::Update(const CNetObj_PlayerInfo *const *ppPlayerInfoById)
{
	for(auto &PlayerData : m_aLockedPlayers)
	{
		if(PlayerData.m_Lock == EPlayerLockState::CONNECTED && !ppPlayerInfoById[PlayerData.m_ClientId])
		{
			PlayerData.m_Lock = EPlayerLockState::DISCONNECTED;
		}
		// TODO handle team switches?
	}
}

bool CScoreboardLock::IsDisconnected(int ArrayId) const
{
	if(!m_IsSet)
		return false;
	return m_aLockedPlayers[ArrayId].m_Lock == EPlayerLockState::DISCONNECTED;
}

bool CScoreboardLock::IsInactive(int ArrayId) const
{
	if(!m_IsSet)
		return false;
	return m_aLockedPlayers[ArrayId].m_Lock == EPlayerLockState::SLOT_EMPTY;
}

void CScoreboardLock::ResetLocks()
{
	for(auto &LockedPlayer : m_aLockedPlayers)
	{
		LockedPlayer.m_aName[0] = '\0';
		LockedPlayer.m_aClan[0] = '\0';
		LockedPlayer.m_ClientId = -1;
		LockedPlayer.m_Lock = EPlayerLockState::SLOT_EMPTY;
		LockedPlayer.m_Country = 0;
		LockedPlayer.m_Team = -2;
	}
}

const CNetObj_PlayerInfo *CScoreboardLock::DisconnectedPlayerInfo(int ArrayId)
{
	m_DisconnectedPlayerInfo.m_ClientId = m_aLockedPlayers[ArrayId].m_ClientId;
	m_DisconnectedPlayerInfo.m_Team = m_aLockedPlayers[ArrayId].m_Team;
	return &m_DisconnectedPlayerInfo;
}

const CGameClient::CClientData *CScoreboardLock::DisconnectedClientData(int ArrayId)
{
	// this is not neat, instead we could make a getter for the name and clan
	str_copy(m_DisconnectedClientData.m_aName, m_aLockedPlayers[ArrayId].m_aName);
	str_copy(m_DisconnectedClientData.m_aClan, m_aLockedPlayers[ArrayId].m_aClan);
	m_DisconnectedClientData.m_Country = m_aLockedPlayers[ArrayId].m_Country;
	return &m_DisconnectedClientData;
}
