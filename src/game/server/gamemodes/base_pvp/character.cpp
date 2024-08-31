#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_pvp.h"

void CGameControllerPvp::OnCharacterConstruct(class CCharacter *pChr)
{
	pChr->m_IsGodmode = false;
}
