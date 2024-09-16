#include <game/server/entities/character.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include <game/server/gamecontext.h>

// implemented in ddracechat.cpp
// yes that is cursed
bool CheckClientId(int ClientId);

void CGameContext::ConStatsRound(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(!pSelf->m_pController)
		return;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chatresp",
		"round stats are not implemnted yet try /statsall");
}

void CGameContext::ConStatsAllTime(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientId(pResult->m_ClientId))
		return;

	if(!pSelf->m_pController)
		return;

	const char *pName = pResult->NumArguments() ? pResult->GetString(0) : pSelf->Server()->ClientName(pResult->m_ClientId);
	pSelf->m_pController->m_pSqlStats->ShowStats(pResult->m_ClientId, pName, pSelf->m_pController->StatsTable());
}
