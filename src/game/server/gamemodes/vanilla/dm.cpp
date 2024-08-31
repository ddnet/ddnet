#include <game/server/entities/character.h>
#include <game/server/player.h>

#include "dm.h"

CGameControllerDM::CGameControllerDM(class CGameContext *pGameServer) :
	CGameControllerVanilla(pGameServer)
{
	m_GameFlags = 0;
	m_pGameType = "DM*";
}

CGameControllerDM::~CGameControllerDM() = default;

void CGameControllerDM::Tick()
{
	CGameControllerVanilla::Tick();
}

void CGameControllerDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerVanilla::OnCharacterSpawn(pChr);
}

int CGameControllerDM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerVanilla::OnCharacterDeath(pVictim, pKiller, WeaponId);

	// check score win condition
	if((m_GameInfo.m_ScoreLimit > 0 && pKiller->m_Score.value_or(0) >= m_GameInfo.m_ScoreLimit) ||
		(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		EndRound();
		return true;
	}
	return false;
}
