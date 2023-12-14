#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "../base_instagib.h"

void CGameControllerInstagib::ConShuffleTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	if(!pSelf->GameServer()->m_pController->IsTeamplay())
		return;

	int rnd = 0;
	int PlayerTeam = 0;
	int aPlayer[MAX_CLIENTS];

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(pSelf->GameServer()->m_apPlayers[i] && pSelf->GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			aPlayer[PlayerTeam++] = i;

	// pSelf->GameServer()->SendGameMsg(GAMEMSG_TEAM_SHUFFLE, -1);

	//creating random permutation
	for(int i = PlayerTeam; i > 1; i--)
	{
		rnd = rand() % i;
		int tmp = aPlayer[rnd];
		aPlayer[rnd] = aPlayer[i - 1];
		aPlayer[i - 1] = tmp;
	}
	//uneven Number of Players?
	rnd = PlayerTeam % 2 ? rand() % 2 : 0;

	for(int i = 0; i < PlayerTeam; i++)
		pSelf->GameServer()->m_pController->DoTeamChange(pSelf->GameServer()->m_apPlayers[aPlayer[i]], i < (PlayerTeam + rnd) / 2 ? TEAM_RED : TEAM_BLUE, false);
}