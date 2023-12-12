#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "../base_instagib.h"

void CGameControllerInstagib::OnCharacterConstruct(class CCharacter *pChr)
{
	pChr->m_IsGodmode = false;
}
