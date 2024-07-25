#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>
#include <game/generated/protocol.h>

#include <base/system.h>

#include "../entities/character.h"
#include "../gamecontext.h"
#include "../gamecontroller.h"
#include "../player.h"

void IGameController::OnEndMatchInsta()
{
	dbg_msg("ddnet-insta", "match end");

	dbg_msg("ddnet-insta", "publishing stats ...");
	PublishRoundEndStats();

	for(CInstaPlayerStats &Stats : m_aInstaPlayerStats)
		Stats.Reset();
}

static float CalcKillDeathRatio(int Kills, int Deaths)
{
	if(!Kills)
		return 0;
	if(!Deaths)
		return (float)Kills;
	return (float)Kills / (float)Deaths;
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
		CalcKillDeathRatio(pStats->m_Kills, pStats->m_Deaths));
	str_append(pBuf, aBuf, Size);
}

void IGameController::GetRoundEndStatsStrJson(char *pBuf, size_t Size)
{
	pBuf[0] = '\0';

	int ScoreRed = m_aTeamscore[TEAM_RED];
	int ScoreBlue = m_aTeamscore[TEAM_BLUE];
	int GameTimeTotalSeconds = (Server()->Tick() - m_GameStartTick) / Server()->TickSpeed();
	int ScoreLimit = m_GameInfo.m_ScoreLimit;
	int TimeLimit = m_GameInfo.m_TimeLimit;

	CJsonStringWriter Writer;
	Writer.BeginObject();
	{
		Writer.WriteAttribute("server");
		Writer.WriteStrValue(g_Config.m_SvName);
		Writer.WriteAttribute("map");
		Writer.WriteStrValue(g_Config.m_SvMap);
		Writer.WriteAttribute("game_type");
		Writer.WriteStrValue(g_Config.m_SvGametype);
		Writer.WriteAttribute("game_duration_seconds");
		Writer.WriteIntValue(GameTimeTotalSeconds);
		Writer.WriteAttribute("score_limit");
		Writer.WriteIntValue(ScoreLimit);
		Writer.WriteAttribute("time_limit");
		Writer.WriteIntValue(TimeLimit);
		Writer.WriteAttribute("score_red");
		Writer.WriteIntValue(ScoreRed);
		Writer.WriteAttribute("score_blue");
		Writer.WriteIntValue(ScoreBlue);

		Writer.WriteAttribute("players");
		Writer.BeginArray();
		for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
		{
			if(!pPlayer)
				continue;

			const CInstaPlayerStats *pStats = &m_aInstaPlayerStats[pPlayer->GetCid()];

			Writer.BeginObject();
			Writer.WriteAttribute("id");
			Writer.WriteIntValue(pPlayer->GetCid());
			Writer.WriteAttribute("team");
			Writer.WriteStrValue(pPlayer->GetTeam() == TEAM_RED ? "red" : "blue");
			Writer.WriteAttribute("name");
			Writer.WriteStrValue(Server()->ClientName(pPlayer->GetCid()));
			Writer.WriteAttribute("score");
			Writer.WriteIntValue(pPlayer->m_Score.value_or(0));
			Writer.WriteAttribute("kills");
			Writer.WriteIntValue(pStats->m_Kills);
			Writer.WriteAttribute("deaths");
			Writer.WriteIntValue(pStats->m_Deaths);
			Writer.WriteAttribute("ratio");
			Writer.WriteIntValue(CalcKillDeathRatio(pStats->m_Kills, pStats->m_Deaths));
			Writer.EndObject();
		}
		Writer.EndArray();
	}
	Writer.EndObject();
	str_copy(pBuf, Writer.GetOutputString().c_str(), Size);
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

	const char *pRedClan = nullptr;
	const char *pBlueClan = nullptr;

	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer)
			continue;

		if(pPlayer->GetTeam() == TEAM_RED)
		{
			if(str_length(Server()->ClientClan(pPlayer->GetCid())) == 0)
			{
				pRedClan = nullptr;
				break;
			}

			if(!pRedClan)
			{
				pRedClan = Server()->ClientClan(pPlayer->GetCid());
				continue;
			}

			if(str_comp(pRedClan, Server()->ClientClan(pPlayer->GetCid())) != 0)
			{
				pRedClan = nullptr;
				break;
			}
		}
		else if(pPlayer->GetTeam() == TEAM_BLUE)
		{
			if(str_length(Server()->ClientClan(pPlayer->GetCid())) == 0)
			{
				pBlueClan = nullptr;
				break;
			}

			if(!pBlueClan)
			{
				pBlueClan = Server()->ClientClan(pPlayer->GetCid());
				continue;
			}

			if(str_comp(pBlueClan, Server()->ClientClan(pPlayer->GetCid())) != 0)
			{
				pBlueClan = nullptr;
				break;
			}
		}
	}

	// headers
	str_format(aBuf, sizeof(aBuf), "---> Server: %s, Map: %s, Gametype: %s.\n", g_Config.m_SvName, g_Config.m_SvMap, g_Config.m_SvGametype);
	str_append(pBuf, aBuf, Size);
	str_format(aBuf, sizeof(aBuf), "(Length: %d min %d sec, Scorelimit: %d, Timelimit: %d)\n\n", GameTimeMinutes, GameTimeSeconds, ScoreLimit, TimeLimit);
	str_append(pBuf, aBuf, Size);

	str_append(pBuf, "**Red Team:**\n", Size);
	if(pRedClan)
	{
		str_format(aBuf, sizeof(aBuf), "Clan: **%s**\n", pRedClan);
		str_append(pBuf, aBuf, Size);
	}
	for(const CPlayer *pPlayer : GameServer()->m_apPlayers)
	{
		if(!pPlayer || pPlayer->GetTeam() != TEAM_RED)
			continue;

		PsvRowPlayer(pPlayer, pBuf, Size);
	}

	str_append(pBuf, "**Blue Team:**\n", Size);
	if(pBlueClan)
	{
		str_format(aBuf, sizeof(aBuf), "Clan: **%s**\n", pBlueClan);
		str_append(pBuf, aBuf, Size);
	}
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

void IGameController::GetRoundEndStatsStrHttp(char *pBuf, size_t Size)
{
	if(!IsTeamPlay())
	{
		dbg_msg("ddnet-insta", "failed to build stats (no teams not implemented)");
		return;
	}

	if(g_Config.m_SvRoundStatsFormatHttp == 0)
		GetRoundEndStatsStrCsv(pBuf, Size);
	if(g_Config.m_SvRoundStatsFormatHttp == 1)
		GetRoundEndStatsStrPsv(pBuf, Size);
	else if(g_Config.m_SvRoundStatsFormatHttp == 4)
		GetRoundEndStatsStrJson(pBuf, Size);
	else
		dbg_msg("ddnet-insta", "sv_round_stats_format_http %d not implemented", g_Config.m_SvRoundStatsFormatHttp);
}

void IGameController::GetRoundEndStatsStrDiscord(char *pBuf, size_t Size)
{
	if(!IsTeamPlay())
	{
		dbg_msg("ddnet-insta", "failed to build stats (no teams not implemented)");
		return;
	}

	if(g_Config.m_SvRoundStatsFormatDiscord == 0)
		GetRoundEndStatsStrCsv(pBuf, Size);
	if(g_Config.m_SvRoundStatsFormatDiscord == 1)
		GetRoundEndStatsStrPsv(pBuf, Size);
	else if(g_Config.m_SvRoundStatsFormatDiscord == 4)
		GetRoundEndStatsStrJson(pBuf, Size);
	else
		dbg_msg("ddnet-insta", "sv_round_stats_format_discord %d not implemented", g_Config.m_SvRoundStatsFormatDiscord);
}

void IGameController::GetRoundEndStatsStrFile(char *pBuf, size_t Size)
{
	if(!IsTeamPlay())
	{
		dbg_msg("ddnet-insta", "failed to build stats (no teams not implemented)");
		return;
	}

	if(g_Config.m_SvRoundStatsFormatFile == 0)
		GetRoundEndStatsStrCsv(pBuf, Size);
	if(g_Config.m_SvRoundStatsFormatFile == 1)
		GetRoundEndStatsStrPsv(pBuf, Size);
	else if(g_Config.m_SvRoundStatsFormatFile == 4)
		GetRoundEndStatsStrJson(pBuf, Size);
	else
		dbg_msg("ddnet-insta", "sv_round_stats_format_file %d not implemented", g_Config.m_SvRoundStatsFormatFile);
}

void IGameController::PublishRoundEndStatsStrFile(const char *pStr)
{
}

void IGameController::PublishRoundEndStatsStrDiscord(const char *pStr)
{
	char aPayload[2048];
	char aStatsStr[2000];
	str_format(
		aPayload,
		sizeof(aPayload),
		"{\"allowed_mentions\": {\"parse\": []}, \"content\": \"%s\"}",
		EscapeJson(aStatsStr, sizeof(aStatsStr), pStr));
	const int PayloadSize = str_length(aPayload);
	// TODO: use HttpPostJson()
	std::shared_ptr<CHttpRequest> pDiscord = HttpPost(g_Config.m_SvRoundStatsDiscordWebhook, (const unsigned char *)aPayload, PayloadSize);
	pDiscord->LogProgress(HTTPLOG::FAILURE);
	pDiscord->IpResolve(IPRESOLVE::V4);
	pDiscord->Timeout(CTimeout{4000, 15000, 500, 5});
	pDiscord->HeaderString("Content-Type", "application/json");
	GameServer()->m_pHttp->Run(pDiscord);
}

void IGameController::PublishRoundEndStatsStrHttp(const char *pStr)
{
	const int PayloadSize = str_length(pStr);
	std::shared_ptr<CHttpRequest> pHttp = HttpPost(g_Config.m_SvRoundStatsHttpEndpoint, (const unsigned char *)pStr, PayloadSize);
	pHttp->LogProgress(HTTPLOG::FAILURE);
	pHttp->IpResolve(IPRESOLVE::V4);
	pHttp->Timeout(CTimeout{4000, 15000, 500, 5});
	if(g_Config.m_SvRoundStatsFormatHttp == 4)
		pHttp->HeaderString("Content-Type", "application/json");
	else
		pHttp->HeaderString("Content-Type", "text/plain");
	// TODO: text/csv
	GameServer()->m_pHttp->Run(pHttp);
}

void IGameController::PublishRoundEndStats()
{
	char aStats[1024];
	if(g_Config.m_SvRoundStatsDiscordWebhook[0] != '\0')
	{
		GetRoundEndStatsStrDiscord(aStats, sizeof(aStats));
		PublishRoundEndStatsStrDiscord(aStats);
		dbg_msg("ddnet-insta", "publishing round stats to discord:\n%s", aStats);
	}
	if(g_Config.m_SvRoundStatsHttpEndpoint[0] != '\0')
	{
		GetRoundEndStatsStrHttp(aStats, sizeof(aStats));
		PublishRoundEndStatsStrHttp(aStats);
		dbg_msg("ddnet-insta", "publishing round stats to custom http endpoint:\n%s", aStats);
	}
	if(g_Config.m_SvRoundStatsOutputFile[0] != '\0')
	{
		GetRoundEndStatsStrFile(aStats, sizeof(aStats));
		PublishRoundEndStatsStrFile(aStats);
		dbg_msg("ddnet-insta", "publishing round stats to file:\n%s", aStats);
	}
}
