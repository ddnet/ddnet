#include <game/server/entities/character.h>
#include <game/server/player.h>

#include "tdm.h"

CGameControllerTDM::CGameControllerTDM(class CGameContext *pGameServer) :
	CGameControllerDM(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS;
}

CGameControllerTDM::~CGameControllerTDM() = default;

void CGameControllerTDM::Tick()
{
	CGameControllerDM::Tick();
}

int CGameControllerTDM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponId);

	if(pKiller && WeaponId != WEAPON_GAME)
	{
		// do team scoring
		if(pKiller == pVictim->GetPlayer() || pKiller->GetTeam() == pVictim->GetPlayer()->GetTeam())
			m_aTeamscore[pKiller->GetTeam() & 1]--;
		else
			m_aTeamscore[pKiller->GetTeam() & 1]++;
	}

	// check score win condition
	if((m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit)) ||
		(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		if(m_SuddenDeath)
		{
			if(m_aTeamscore[TEAM_RED] / 100 != m_aTeamscore[TEAM_BLUE] / 100)
			{
				EndRound();
				return true;
			}
		}
		else
		{
			if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE])
			{
				EndRound();
				return true;
			}
			else
				m_SuddenDeath = 1;
		}
	}
	return false;
}

void CGameControllerTDM::Snap(int SnappingClient)
{
	CGameControllerInstagib::Snap(SnappingClient);

	if(Server()->IsSixup(SnappingClient))
		return;

	CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if(!pGameDataObj)
		return;

	pGameDataObj->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
	pGameDataObj->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];

	pGameDataObj->m_FlagCarrierRed = 0;
	pGameDataObj->m_FlagCarrierBlue = 0;
}
