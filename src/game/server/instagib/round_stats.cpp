#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>

#include <base/system.h>

#include "../entities/character.h"
#include "../gamecontext.h"
#include "../gamecontroller.h"
#include "../gamemodes/DDRace.h"
#include "../gamemodes/gctf.h"
#include "../gamemodes/ictf.h"
#include "../gamemodes/mod.h"
#include "../player.h"

void IGameController::OnEndMatchInsta()
{
	dbg_msg("ddnet-insta", "match end");

	char aStats[1024];
	dbg_msg("ddnet-insta", "building stats ...");
	GetRoundEndStatsStr(aStats, sizeof(aStats));
	PublishRoundEndStatsStr(aStats);
}

void IGameController::PsvRowPlayer(const CPlayer *pPlayer, char *pBuf, size_t Size)
{
	char aBuf[512];
	const CInstaPlayerStats *pStats = &m_aInstaPlayerStats[pPlayer->GetCid()];
	str_format(
		aBuf,
		sizeof(aBuf),
		"Id: %d | Name: %s | Score: %d | Kills: %d | Deaths: %d | Ratio: %.2f\n",
		pPlayer->GetCid(),
		Server()->ClientName(pPlayer->GetCid()),
		pPlayer->m_Score.value_or(0),
		pStats->m_Kills,
		pStats->m_Deaths,
		(float)pStats->m_Kills / ((float)pStats->m_Kills + (float)pStats->m_Deaths));
	str_append(pBuf, aBuf, Size);
}

void IGameController::GetRoundEndStatsStrPsv(char *pBuf, size_t Size)
{
	pBuf[0] = '\0';
	char aBuf[512];

	int ScoreRed = m_aTeamscore[TEAM_RED];
	int ScoreBlue = m_aTeamscore[TEAM_BLUE];
	int GameTimeTotalSeconds = (Server()->Tick() - m_GameStartTick) / Server()->TickSpeed();
	int GameTimeMinutes = GameTimeTotalSeconds / 60;
	int GameTimeSeconds = GameTimeTotalSeconds % 60;
	int ScoreLimit = m_GameInfo.m_ScoreLimit;
	int TimeLimit = m_GameInfo.m_TimeLimit;

	// headers
	str_format(aBuf, sizeof(aBuf), "---> Server: %s, Map: %s, Gametype: %s.\n", g_Config.m_SvName, g_Config.m_SvMap, g_Config.m_SvGametype);
	str_append(pBuf, aBuf, Size);
	str_format(aBuf, sizeof(aBuf), "(Length: %d min %d sec, Scorelimit: %d, Timelimit: %d)\n\n", GameTimeMinutes, GameTimeSeconds, ScoreLimit, TimeLimit);
	str_append(pBuf, aBuf, Size);

	str_append(pBuf, "**Red Team:**\n", Size);
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer || pPlayer->GetTeam() != TEAM_RED)
			continue;

		PsvRowPlayer(pPlayer, pBuf, Size);
	}
	str_append(pBuf, "**Blue Team:**\n", Size);
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer || pPlayer->GetTeam() != TEAM_BLUE)
			continue;

		PsvRowPlayer(pPlayer, pBuf, Size);
	}

	str_append(pBuf, "---------------------\n", Size);
	str_format(aBuf, sizeof(aBuf), "**Red: %d | Blue %d**\n", ScoreRed, ScoreBlue);
	str_append(pBuf, aBuf, Size);
}

void IGameController::GetRoundEndStatsStrAsciiTable(char *pBuf, size_t Size)
{
}

void IGameController::GetRoundEndStatsStr(char *pBuf, size_t Size)
{
	if(!IsTeamPlay())
	{
		dbg_msg("ddnet-insta", "failed to build stats (no teams not implemented)");
		return;
	}

	if(g_Config.m_SvRoundStatsFormat == 0)
		GetRoundEndStatsStrCsv(pBuf, Size);
	if(g_Config.m_SvRoundStatsFormat == 1)
		GetRoundEndStatsStrPsv(pBuf, Size);
	else
		dbg_msg("ddnet-insta", "sv_round_stats_format %d not implemented", g_Config.m_SvRoundStatsFormat);
}

void IGameController::PublishRoundEndStatsStrFile(const char *pStr)
{
}

void IGameController::PublishRoundEndStatsStrDiscord(const char *pStr)
{
	char aPayload[2048];
	char aStatsStr[2000];
	str_format(aPayload, sizeof(aPayload), "{\"content\": \"%s\"}", EscapeJson(aStatsStr, sizeof(aStatsStr), pStr));
	const int PayloadSize = str_length(aPayload);
	// TODO: use HttpPostJson()
	std::shared_ptr<CHttpRequest> pDiscord = HttpPost(g_Config.m_SvRoundStatsDiscordWebhook, (const unsigned char *)aPayload, PayloadSize);
	pDiscord->LogProgress(HTTPLOG::FAILURE);
	pDiscord->IpResolve(IPRESOLVE::V4);
	pDiscord->Timeout(CTimeout{4000, 15000, 500, 5});
	pDiscord->HeaderString("Content-Type", "application/json");
	GameServer()->m_pHttp->Run(pDiscord);
}

void IGameController::PublishRoundEndStatsStr(const char *pStr)
{
	dbg_msg("ddnet-insta", "publishing round stats:\n%s", pStr);
	if(g_Config.m_SvRoundStatsDiscordWebhook[0] != '\0')
		PublishRoundEndStatsStrDiscord(pStr);
	if(g_Config.m_SvRoundStatsOutputFile[0] != '\0')
		PublishRoundEndStatsStrFile(pStr);
}
