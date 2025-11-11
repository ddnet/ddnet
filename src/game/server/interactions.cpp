#include "interactions.h"

#include <engine/shared/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

const CPlayer *CInteractions::GetPlayer(const CGameContext *pGameServer, int ClientId) const
{
	dbg_assert(ClientId >= 0 && ClientId < MAX_CLIENTS, "invalid client id %d", ClientId);
	return pGameServer->m_apPlayers[ClientId];
}

const CCharacter *CInteractions::Character(const CGameContext *pGameServer, int ClientId) const
{
	return pGameServer->GetPlayerChar(ClientId);
}

bool CInteractions::IsSolo(const CGameContext *pGameServer, int ClientId) const
{
	const CCharacter *pChr = Character(pGameServer, ClientId);
	return pChr && pChr->Core()->m_Solo;
}

int CInteractions::GetDDRaceTeam(const CGameContext *pGameServer, int ClientId) const
{
	return pGameServer->GetDDRaceTeam(ClientId);
}

void CInteractions::Init(int OwnerId, uint32_t UniqueOwnerId)
{
	m_OwnerId = OwnerId;
	m_UniqueOwnerId = UniqueOwnerId;
}

void CInteractions::FillOwnerConnected(
	bool OwnerAlive,
	int DDRaceTeam,
	bool Solo,
	bool NoHitOthers,
	bool NoHitSelf)
{
	m_OwnerAlive = OwnerAlive;
	m_DDRaceTeam = DDRaceTeam;
	m_Solo = Solo;
	m_NoHitOthers = NoHitOthers;
	m_NoHitSelf = NoHitSelf;
}

void CInteractions::FillOwnerDisconnected()
{
	m_OwnerAlive = false;
	m_OwnerId = -1;
	// m_UniqueOwnerId = 0; // TODO: yes no maybe?
}

bool CInteractions::CanSee(const CGameContext *pGameServer, int ClientId) const
{
	const CPlayer *pPlayer = GetPlayer(pGameServer, ClientId);
	if(!pPlayer)
		return false; // Player doesn't exist

	if(!(pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()))
	{ // Not spectator
		if(ClientId != m_OwnerId)
		{ // Actions of other players
			if(!Character(pGameServer, ClientId))
				return false; // Player is currently dead
			if(pPlayer->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
			{
				if(GetDDRaceTeam(pGameServer, ClientId) != m_DDRaceTeam && GetDDRaceTeam(pGameServer, ClientId) != TEAM_SUPER)
					return false; // In different teams
			}
			else if(pPlayer->m_ShowOthers == SHOW_OTHERS_OFF)
			{
				if(m_Solo)
					return false; // When in solo part don't show others
				if(IsSolo(pGameServer, ClientId))
					return false; // When in solo part don't show others
				if(GetDDRaceTeam(pGameServer, ClientId) != m_DDRaceTeam && GetDDRaceTeam(pGameServer, ClientId) != TEAM_SUPER)
					return false; // In different teams
			}
		} // See everything of yourself
	}
	else if(pPlayer->SpectatorId() != SPEC_FREEVIEW)
	{ // Spectating specific player
		if(pPlayer->SpectatorId() != m_OwnerId)
		{ // Actions of other players
			if(!Character(pGameServer, pPlayer->SpectatorId()))
				return false; // Player is currently dead
			if(pPlayer->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
			{
				if(GetDDRaceTeam(pGameServer, pPlayer->SpectatorId()) != m_DDRaceTeam && GetDDRaceTeam(pGameServer, pPlayer->SpectatorId()) != TEAM_SUPER)
					return false; // In different teams
			}
			else if(pPlayer->m_ShowOthers == SHOW_OTHERS_OFF)
			{
				if(m_Solo)
					return false; // When in solo part don't show others
				if(IsSolo(pGameServer, pPlayer->SpectatorId()))
					return false; // When in solo part don't show others
				if(GetDDRaceTeam(pGameServer, pPlayer->SpectatorId()) != m_DDRaceTeam && GetDDRaceTeam(pGameServer, pPlayer->SpectatorId()) != TEAM_SUPER)
					return false; // In different teams
			}
		} // See everything of player you're spectating
	}
	else
	{ // Freeview
		if(pPlayer->m_SpecTeam)
		{ // Show only players in own team when spectating
			if(GetDDRaceTeam(pGameServer, ClientId) != m_DDRaceTeam && GetDDRaceTeam(pGameServer, ClientId) != TEAM_SUPER)
				return false; // in different teams
		}
	}

	return true;
}

bool CInteractions::CanHit(const CGameContext *pGameServer, int ClientId) const
{
	const CPlayer *pPlayer = GetPlayer(pGameServer, ClientId);
	if(!pPlayer)
		return false;

	if(m_DDRaceTeam && GetDDRaceTeam(pGameServer, ClientId) != m_DDRaceTeam)
		return false;
	if(m_Solo && m_UniqueOwnerId != pPlayer->GetUniqueCid())
		return false;
	if(m_NoHitOthers && m_UniqueOwnerId != pPlayer->GetUniqueCid())
		return false;
	if(m_NoHitSelf && m_UniqueOwnerId == pPlayer->GetUniqueCid())
		return false;

	return true;
}

CClientMask CInteractions::CanSeeMask(const CGameContext *pGameServer) const
{
	if(m_DDRaceTeam == TEAM_SUPER)
	{
		return CClientMask().set();
	}

	CClientMask Mask;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(CanSee(pGameServer, i))
		{
			Mask.set(i);
		}
	}
	return Mask;
}

CClientMask CInteractions::CanHitMask(const CGameContext *pGameServer) const
{
	if(m_DDRaceTeam == TEAM_SUPER)
	{
		return CClientMask().set();
	}

	CClientMask Mask;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(CanHit(pGameServer, i))
		{
			Mask.set(i);
		}
	}
	return Mask;
}
