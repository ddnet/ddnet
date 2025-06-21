#include "interactions.h"

#include <engine/shared/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CGameContext *CInteractions::GameServer()
{
	return m_pGameServer;
}

CPlayer *CInteractions::GetPlayer(int ClientId)
{
	dbg_assert(ClientId >= 0 && ClientId < MAX_CLIENTS, "invalid client id %d", ClientId);
	return GameServer()->m_apPlayers[ClientId];
}

CCharacter *CInteractions::Character(int ClientId)
{
	return GameServer()->GetPlayerChar(ClientId);
}

bool CInteractions::IsSolo(int ClientId)
{
	CCharacter *pChr = Character(ClientId);
	return pChr && pChr->Core()->m_Solo;
}

int CInteractions::GetDDRaceTeam(int ClientId)
{
	return GameServer()->GetDDRaceTeam(ClientId);
}

void CInteractions::Init(class CGameContext *pGameServer, int OwnerId, uint32_t UniqueOwnerId)
{
	m_pGameServer = pGameServer;
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

bool CInteractions::CanSee(int ClientId)
{
	CPlayer *pPlayer = GetPlayer(ClientId);
	if(!pPlayer)
		return false; // Player doesn't exist

	if(!(pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()))
	{ // Not spectator
		if(ClientId != m_OwnerId)
		{ // Actions of other players
			if(!Character(ClientId))
				return false; // Player is currently dead
			if(pPlayer->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
			{
				if(GetDDRaceTeam(ClientId) != m_DDRaceTeam && GetDDRaceTeam(ClientId) != TEAM_SUPER)
					return false; // In different teams
			}
			else if(pPlayer->m_ShowOthers == SHOW_OTHERS_OFF)
			{
				if(m_Solo)
					return false; // When in solo part don't show others
				if(IsSolo(ClientId))
					return false; // When in solo part don't show others
				if(GetDDRaceTeam(ClientId) != m_DDRaceTeam && GetDDRaceTeam(ClientId) != TEAM_SUPER)
					return false; // In different teams
			}
		} // See everything of yourself
	}
	else if(pPlayer->SpectatorId() != SPEC_FREEVIEW)
	{ // Spectating specific player
		if(pPlayer->SpectatorId() != m_OwnerId)
		{ // Actions of other players
			if(!Character(pPlayer->SpectatorId()))
				return false; // Player is currently dead
			if(pPlayer->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
			{
				if(GetDDRaceTeam(pPlayer->SpectatorId()) != m_DDRaceTeam && GetDDRaceTeam(pPlayer->SpectatorId()) != TEAM_SUPER)
					return false; // In different teams
			}
			else if(pPlayer->m_ShowOthers == SHOW_OTHERS_OFF)
			{
				if(m_Solo)
					return false; // When in solo part don't show others
				if(IsSolo(pPlayer->SpectatorId()))
					return false; // When in solo part don't show others
				if(GetDDRaceTeam(pPlayer->SpectatorId()) != m_DDRaceTeam && GetDDRaceTeam(pPlayer->SpectatorId()) != TEAM_SUPER)
					return false; // In different teams
			}
		} // See everything of player you're spectating
	}
	else
	{ // Freeview
		if(pPlayer->m_SpecTeam)
		{ // Show only players in own team when spectating
			if(GetDDRaceTeam(ClientId) != m_DDRaceTeam && GetDDRaceTeam(ClientId) != TEAM_SUPER)
				return false; // in different teams
		}
	}

	return true;
}

bool CInteractions::CanHit(int ClientId)
{
	CPlayer *pPlayer = GetPlayer(ClientId);
	if(!pPlayer)
		return false;

	if(m_DDRaceTeam && GetDDRaceTeam(ClientId) != m_DDRaceTeam)
		return false;
	if(m_Solo && m_UniqueOwnerId != pPlayer->GetUniqueCid())
		return false;
	if(m_NoHitOthers && m_UniqueOwnerId != pPlayer->GetUniqueCid())
		return false;
	if(m_NoHitSelf && m_UniqueOwnerId == pPlayer->GetUniqueCid())
		return false;

	return true;
}

CClientMask CInteractions::CanSeeMask()
{
	if(m_DDRaceTeam == TEAM_SUPER)
	{
		return CClientMask().set();
	}

	CClientMask Mask;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(CanSee(i))
		{
			Mask.set(i);
		}
	}
	return Mask;
}

CClientMask CInteractions::CanHitMask()
{
	if(m_DDRaceTeam == TEAM_SUPER)
	{
		return CClientMask().set();
	}

	CClientMask Mask;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(CanHit(i))
		{
			Mask.set(i);
		}
	}
	return Mask;
}
