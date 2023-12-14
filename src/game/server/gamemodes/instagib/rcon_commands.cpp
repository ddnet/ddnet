#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "../base_instagib.h"

void CGameControllerInstagib::ConHammer(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_HAMMER, false);
}

void CGameControllerInstagib::ConGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GUN, false);
}

void CGameControllerInstagib::ConUnHammer(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_HAMMER, true);
}

void CGameControllerInstagib::ConUnGun(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	pSelf->ModifyWeapons(pResult, pUserData, WEAPON_GUN, true);
}

void CGameControllerInstagib::ConGodmode(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	int Victim = pResult->GetVictim();

	CCharacter *pChr = pSelf->GameServer()->GetPlayerChar(Victim);

	if(!pChr)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' got godmode!",
		pSelf->GameServer()->Server()->ClientName(Victim));
	pSelf->GameServer()->SendChat(-1, pSelf->GameServer()->CHAT_ALL, aBuf);

	pChr->m_IsGodmode = true;
}

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

void CGameControllerInstagib::ConSwapTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	pSelf->SwapTeams();
}

void CGameControllerInstagib::ConSwapTeamsRandom(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerInstagib *pSelf = (CGameControllerInstagib *)pUserData;
	if(rand() % 2)
		pSelf->SwapTeams();
	else
		dbg_msg("swap", "did not swap due to random chance");
}

void CGameControllerInstagib::SwapTeams()
{
	if(!GameServer()->m_pController->IsTeamplay())
		return;

	GameServer()->SendGameMsg(protocol7::GAMEMSG_TEAM_SWAP, -1);

	for(CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS)
			GameServer()->m_pController->DoTeamChange(pPlayer, pPlayer->GetTeam() ^ 1, false);
	}

	GameServer()->m_pController->SwapTeamscore();
}