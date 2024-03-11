#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "../base_instagib.h"

void CGameControllerInstagib::OnPlayerConstruct(class CPlayer *pPlayer)
{
	pPlayer->m_IsDead = false;
	pPlayer->m_KillerId = -1;
	pPlayer->m_Spree = 0;
}
