#include <game/server/entities/character.h>
#include <game/server/player.h>

#include "dm.h"

CGameControllerBaseDM::CGameControllerBaseDM(class CGameContext *pGameServer) :
	CGameControllerPvp(pGameServer)
{
	m_GameFlags = 0;
}

CGameControllerBaseDM::~CGameControllerBaseDM() = default;

void CGameControllerBaseDM::Tick()
{
	CGameControllerPvp::Tick();
}

void CGameControllerBaseDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerPvp::OnCharacterSpawn(pChr);
}

int CGameControllerBaseDM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerPvp::OnCharacterDeath(pVictim, pKiller, WeaponId);

	// check score win condition
	if((m_GameInfo.m_ScoreLimit > 0 && pKiller->m_Score.value_or(0) >= m_GameInfo.m_ScoreLimit) ||
		(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		EndRound();
		return true;
	}
	return false;
}
