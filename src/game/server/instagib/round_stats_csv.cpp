#include <engine/shared/config.h>

#include "../entities/character.h"
#include "../gamecontext.h"
#include "../gamecontroller.h"
#include "../gamemodes/DDRace.h"
#include "../gamemodes/gctf.h"
#include "../gamemodes/ictf.h"
#include "../gamemodes/mod.h"
#include "../player.h"

#include "round_stats_player.h"

void IGameController::GetRoundEndStatsStrCsv(char *pBuf, size_t Size)
{
	pBuf[0] = '\0';
	char aBuf[512];

	int ScoreRed = m_aTeamscore[TEAM_RED];
	int ScoreBlue = m_aTeamscore[TEAM_BLUE];

	// csv header
	str_append(pBuf, "red_name, red_score, blue_name, blue_score\n", Size);

	// insert blue and red as if they were players called blue and red
	// as the first entry
	str_format(aBuf, sizeof(aBuf), "red, %d, blue, %d\n", ScoreRed, ScoreBlue);
	str_append(pBuf, aBuf, Size);

	CStatsPlayer aStatsPlayerRed[MAX_CLIENTS];
	CStatsPlayer aStatsPlayerBlue[MAX_CLIENTS];

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer)
			continue;

		CStatsPlayer *pStatsPlayer = pPlayer->GetTeam() == TEAM_RED ? &aStatsPlayerRed[i] : &aStatsPlayerBlue[i];
		pStatsPlayer->m_Active = true;
		pStatsPlayer->m_Score = pPlayer->m_Score.value_or(0);
		pStatsPlayer->m_pName = Server()->ClientName(pPlayer->GetCID());
	}

	std::stable_sort(aStatsPlayerRed, aStatsPlayerRed + MAX_CLIENTS,
		[](const CStatsPlayer &p1, const CStatsPlayer &p2) -> bool {
			return p1.m_Score > p2.m_Score;
		});
	std::stable_sort(aStatsPlayerBlue, aStatsPlayerBlue + MAX_CLIENTS,
		[](const CStatsPlayer &p1, const CStatsPlayer &p2) -> bool {
			return p1.m_Score > p2.m_Score;
		});

	int RedIndex = 0;
	int BlueIndex = 0;
	const CStatsPlayer *pRed = &aStatsPlayerRed[RedIndex];
	const CStatsPlayer *pBlue = &aStatsPlayerBlue[BlueIndex];
	while(pRed || pBlue)
	{
		// mom: "we have pop() at home"
		// the pop() at home:
		while(pRed && !pRed->m_Active)
		{
			if(++RedIndex >= MAX_CLIENTS)
			{
				pRed = nullptr;
				break;
			};
			pRed = &aStatsPlayerRed[RedIndex];
		}
		while(pBlue && !pBlue->m_Active)
		{
			if(++BlueIndex >= MAX_CLIENTS)
			{
				pBlue = nullptr;
				break;
			};
			pBlue = &aStatsPlayerBlue[BlueIndex];
		}
		if(!pBlue && !pRed)
			break;

		// dbg_msg("debug", "RedIndex=%d BlueIndex=%d pRed=%p pBlue=%p", RedIndex, BlueIndex, pRed, pBlue);

		str_format(
			aBuf,
			sizeof(aBuf),
			"%s, %d, %s, %d\n",
			pRed ? pRed->m_pName : "",
			pRed ? pRed->m_Score : 0,
			pBlue ? pBlue->m_pName : "",
			pBlue ? pBlue->m_Score : 0);
		str_append(pBuf, aBuf, Size);

		if(++RedIndex >= MAX_CLIENTS)
		{
			pRed = nullptr;
		}
		else
		{
			pRed = &aStatsPlayerRed[RedIndex];
		}
		if(++BlueIndex >= MAX_CLIENTS)
		{
			pBlue = nullptr;
		}
		else
		{
			pBlue = &aStatsPlayerBlue[BlueIndex];
		}
	}
}
