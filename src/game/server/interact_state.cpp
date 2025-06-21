#include <engine/shared/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "interact_state.h"

CGameContext *CInteractState::GameServer()
{
	return m_pGameServer;
}

CPlayer *CInteractState::GetPlayer(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return nullptr;
	return GameServer()->m_apPlayers[ClientId];
}

CCharacter *CInteractState::Character(int ClientId)
{
	return GameServer()->GetPlayerChar(ClientId);
}

bool CInteractState::IsSolo(int ClientId)
{
	CCharacter *pChr = Character(ClientId);
	return pChr && pChr->Core()->m_Solo;
}

int CInteractState::GetDDRaceTeam(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return 0;
	return GameServer()->GetDDRaceTeam(ClientId);
}

void CInteractState::Init(class CGameContext *pGameServer, int OwnerId, int UniqueOwnerId)
{
	m_pGameServer = pGameServer;
	m_OwnerId = OwnerId;
	m_UniqueOwnerId = UniqueOwnerId;
}

void CInteractState::FillOwnerConnected(
	bool OwnerAlive,
	int DDRaceTeam,
	bool Solo,
	bool NoHit)
{
	m_OwnerAlive = OwnerAlive;
	m_DDRaceTeam = DDRaceTeam;
	m_Solo = Solo;
	m_NoHit = NoHit;
}

void CInteractState::FillOwnerDisconnected()
{
	m_OwnerAlive = false;
	m_OwnerId = -1;
	// m_UniqueOwnerId = 0; // TODO: yes no maybe?
}

bool CInteractState::CanSee(int ClientId, CGameContext *pGameServer) const
{
	CPlayer *pPlayer = pGameServer->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;

	return true;
}

bool CInteractState::CanHit(int ClientId, CGameContext *pGameServer) const
{
	CPlayer *pPlayer = pGameServer->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;

	if(m_DDRaceTeam)
	{
		if(pGameServer->GetDDRaceTeam(ClientId) != m_DDRaceTeam)
			return false;
	}

	return true;
}

CClientMask CInteractState::CanSeeMask()
{
	if(m_DDRaceTeam == TEAM_SUPER)
	{
		return CClientMask().set();
	}

	CClientMask Mask;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!GetPlayer(i))
			continue; // Player doesn't exist

		if(!(GetPlayer(i)->GetTeam() == TEAM_SPECTATORS || GetPlayer(i)->IsPaused()))
		{ // Not spectator
			if(i != m_OwnerId)
			{ // Actions of other players
				if(!Character(i))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
				{
					if(GetDDRaceTeam(i) != m_DDRaceTeam && GetDDRaceTeam(i) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_OFF)
				{
					if(m_Solo)
						continue; // When in solo part don't show others
					if(IsSolo(i))
						continue; // When in solo part don't show others
					if(GetDDRaceTeam(i) != m_DDRaceTeam && GetDDRaceTeam(i) != TEAM_SUPER)
						continue; // In different teams
				}
			} // See everything of yourself
		}
		else if(GetPlayer(i)->m_SpectatorId != SPEC_FREEVIEW)
		{ // Spectating specific player
			if(GetPlayer(i)->m_SpectatorId != m_OwnerId)
			{ // Actions of other players
				if(!Character(GetPlayer(i)->m_SpectatorId))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
				{
					if(GetDDRaceTeam(GetPlayer(i)->m_SpectatorId) != m_DDRaceTeam && GetDDRaceTeam(GetPlayer(i)->m_SpectatorId) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_OFF)
				{
					if(m_Solo)
						continue; // When in solo part don't show others
					if(IsSolo(GetPlayer(i)->m_SpectatorId))
						continue; // When in solo part don't show others
					if(GetDDRaceTeam(GetPlayer(i)->m_SpectatorId) != m_DDRaceTeam && GetDDRaceTeam(GetPlayer(i)->m_SpectatorId) != TEAM_SUPER)
						continue; // In different teams
				}
			} // See everything of player you're spectating
		}
		else
		{ // Freeview
			if(GetPlayer(i)->m_SpecTeam)
			{ // Show only players in own team when spectating
				if(GetDDRaceTeam(i) != m_DDRaceTeam && GetDDRaceTeam(i) != TEAM_SUPER)
					continue; // in different teams
			}
		}

		Mask.set(i);
	}
	return Mask;
}
