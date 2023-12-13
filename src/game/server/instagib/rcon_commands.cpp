#include <engine/shared/config.h>

#include "../entities/character.h"
#include "../gamecontext.h"
#include "../gamemodes/DDRace.h"
#include "../gamemodes/gctf.h"
#include "../gamemodes/ictf.h"
#include "../gamemodes/mod.h"
#include "../player.h"

#include "strhelpers.h"

// gctf
void CGameContext::ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		if(pSelf->m_pController)
			pSelf->m_pController->CheckGameInfo();
	}
}

// gctf
void CGameContext::ConchainResetInstasettingTees(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
	{
		for(auto *pPlayer : pSelf->m_apPlayers)
		{
			if(!pPlayer)
				continue;
			CCharacter *pChr = pPlayer->GetCharacter();
			if(!pChr)
				continue;
			pChr->ResetInstaSettings();
		}
	}
}

// gctf
void CGameContext::ConchainInstaSettingsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->UpdateVoteCheckboxes();
	pSelf->RefreshVotes();
}

// gctf
void CGameContext::ConShuffleTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->m_pController->IsTeamplay())
		return;

	int rnd = 0;
	int PlayerTeam = 0;
	int aPlayer[MAX_CLIENTS];

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			aPlayer[PlayerTeam++] = i;

	// pSelf->SendGameMsg(GAMEMSG_TEAM_SHUFFLE, -1);

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
		pSelf->m_pController->DoTeamChange(pSelf->m_apPlayers[aPlayer[i]], i < (PlayerTeam + rnd) / 2 ? TEAM_RED : TEAM_BLUE, false);
}

void CGameContext::ConSwapTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SwapTeams();
}

void CGameContext::ConSwapTeamsRandom(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(rand() % 2)
		pSelf->SwapTeams();
	else
		dbg_msg("swap", "did not swap due to random chance");
}

void CGameContext::SwapTeams()
{
	if(!m_pController->IsTeamplay())
		return;

	SendGameMsg(protocol7::GAMEMSG_TEAM_SWAP, -1);

	for(CPlayer *pPlayer : m_apPlayers)
	{
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS)
			m_pController->DoTeamChange(pPlayer, pPlayer->GetTeam() ^ 1, false);
	}

	m_pController->SwapTeamscore();
}