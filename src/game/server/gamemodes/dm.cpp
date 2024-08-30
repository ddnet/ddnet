#include <game/server/entities/character.h>
#include <game/server/player.h>

#include "dm.h"

CGameControllerDM::CGameControllerDM(class CGameContext *pGameServer) :
	CGameControllerInstagib(pGameServer)
{
	m_GameFlags = 0;
}

CGameControllerDM::~CGameControllerDM() = default;

void CGameControllerDM::Tick()
{
	CGameControllerInstagib::Tick();
}

void CGameControllerDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstagib::OnCharacterSpawn(pChr);
}

int CGameControllerDM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponId);

	// check score win condition
	if((m_GameInfo.m_ScoreLimit > 0 && pKiller->m_Score.value_or(0) >= m_GameInfo.m_ScoreLimit) ||
		(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		EndRound();
		return true;
	}
	return false;
}
