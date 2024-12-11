#include "score.h"
#include "gamemodes/DDRace.h"
#include "player.h"
#include "save.h"
#include "scoreworker.h"

#include <base/system.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/shared/config.h>
#include <engine/shared/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <game/generated/wordlist.h>

#include <memory>

class IDbConnection;

std::shared_ptr<CScorePlayerResult> CScore::NewSqlPlayerResult(int ClientId)
{
	CPlayer *pCurPlayer = GameServer()->m_apPlayers[ClientId];
	if(pCurPlayer->m_ScoreQueryResult != nullptr) // TODO: send player a message: "too many requests"
		return nullptr;
	pCurPlayer->m_ScoreQueryResult = std::make_shared<CScorePlayerResult>();
	return pCurPlayer->m_ScoreQueryResult;
}

void CScore::ExecPlayerThread(
	bool (*pFuncPtr)(IDbConnection *, const ISqlData *, char *pError, int ErrorSize),
	const char *pThreadName,
	int ClientId,
	const char *pName,
	int Offset)
{
	auto pResult = NewSqlPlayerResult(ClientId);
	if(pResult == nullptr)
		return;
	auto Tmp = std::make_unique<CSqlPlayerRequest>(pResult);
	str_copy(Tmp->m_aName, pName, sizeof(Tmp->m_aName));
	str_copy(Tmp->m_aMap, Server()->GetMapName(), sizeof(Tmp->m_aMap));
	str_copy(Tmp->m_aServer, g_Config.m_SvSqlServerName, sizeof(Tmp->m_aServer));
	str_copy(Tmp->m_aRequestingPlayer, Server()->ClientName(ClientId), sizeof(Tmp->m_aRequestingPlayer));
	Tmp->m_Offset = Offset;

	m_pPool->Execute(pFuncPtr, std::move(Tmp), pThreadName);
}

bool CScore::RateLimitPlayer(int ClientId)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(pPlayer == 0)
		return true;
	if(pPlayer->m_LastSqlQuery + (int64_t)g_Config.m_SvSqlQueriesDelay * Server()->TickSpeed() >= Server()->Tick())
		return true;
	pPlayer->m_LastSqlQuery = Server()->Tick();
	return false;
}

void CScore::GeneratePassphrase(char *pBuf, int BufSize)
{
	for(int i = 0; i < 3; i++)
	{
		if(i != 0)
			str_append(pBuf, " ", BufSize);
		// TODO: decide if the slight bias towards lower numbers is ok
		int Rand = m_Prng.RandomBits() % m_vWordlist.size();
		str_append(pBuf, m_vWordlist[Rand].c_str(), BufSize);
	}
}

CScore::CScore(CGameContext *pGameServer, CDbConnectionPool *pPool) :
	m_pPool(pPool),
	m_pGameServer(pGameServer),
	m_pServer(pGameServer->Server())
{
	LoadBestTime();

	uint64_t aSeed[2];
	secure_random_fill(aSeed, sizeof(aSeed));
	m_Prng.Seed(aSeed);

	CLineReader LineReader;
	if(LineReader.OpenFile(GameServer()->Storage()->OpenFile("wordlist.txt", IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		while(const char *pLine = LineReader.Get())
		{
			char aWord[32] = {0};
			sscanf(pLine, "%*s %31s", aWord);
			aWord[31] = 0;
			m_vWordlist.emplace_back(aWord);
		}
	}
	else
	{
		dbg_msg("sql", "failed to open wordlist, using fallback");
		m_vWordlist.assign(std::begin(g_aFallbackWordlist), std::end(g_aFallbackWordlist));
	}

	if(m_vWordlist.size() < 1000)
	{
		dbg_msg("sql", "too few words in wordlist");
		Server()->SetErrorShutdown("sql too few words in wordlist");
		return;
	}
}

void CScore::LoadBestTime()
{
	if(m_pGameServer->m_pController->m_pLoadBestTimeResult)
		return; // already in progress

	auto LoadBestTimeResult = std::make_shared<CScoreLoadBestTimeResult>();
	m_pGameServer->m_pController->m_pLoadBestTimeResult = LoadBestTimeResult;

	auto Tmp = std::make_unique<CSqlLoadBestTimeRequest>(LoadBestTimeResult);
	str_copy(Tmp->m_aMap, Server()->GetMapName(), sizeof(Tmp->m_aMap));
	m_pPool->Execute(CScoreWorker::LoadBestTime, std::move(Tmp), "load best time");
}

void CScore::LoadPlayerData(int ClientId, const char *pName)
{
	ExecPlayerThread(CScoreWorker::LoadPlayerData, "load player data", ClientId, pName, 0);
}

void CScore::LoadPlayerTimeCp(int ClientId, const char *pName)
{
	ExecPlayerThread(CScoreWorker::LoadPlayerTimeCp, "load player timecp", ClientId, pName, 0);
}

void CScore::MapVote(int ClientId, const char *pMapName)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::MapVote, "map vote", ClientId, pMapName, 0);
}

void CScore::MapInfo(int ClientId, const char *pMapName)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::MapInfo, "map info", ClientId, pMapName, 0);
}

void CScore::SaveScore(int ClientId, int TimeTicks, const char *pTimestamp, const float aTimeCp[NUM_CHECKPOINTS], bool NotEligible)
{
	CConsole *pCon = (CConsole *)GameServer()->Console();
	if(pCon->Cheated() || NotEligible)
		return;

	GameServer()->TeehistorianRecordPlayerFinish(ClientId, TimeTicks);

	CPlayer *pCurPlayer = GameServer()->m_apPlayers[ClientId];
	if(pCurPlayer->m_ScoreFinishResult != nullptr)
		dbg_msg("sql", "WARNING: previous save score result didn't complete, overwriting it now");
	pCurPlayer->m_ScoreFinishResult = std::make_shared<CScorePlayerResult>();
	auto Tmp = std::make_unique<CSqlScoreData>(pCurPlayer->m_ScoreFinishResult);
	str_copy(Tmp->m_aMap, Server()->GetMapName(), sizeof(Tmp->m_aMap));
	FormatUuid(GameServer()->GameUuid(), Tmp->m_aGameUuid, sizeof(Tmp->m_aGameUuid));
	Tmp->m_ClientId = ClientId;
	str_copy(Tmp->m_aName, Server()->ClientName(ClientId), sizeof(Tmp->m_aName));
	Tmp->m_Time = (float)(TimeTicks) / (float)Server()->TickSpeed();
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
		Tmp->m_aCurrentTimeCp[i] = aTimeCp[i];

	m_pPool->ExecuteWrite(CScoreWorker::SaveScore, std::move(Tmp), "save score");
}

void CScore::SaveTeamScore(int Team, int *pClientIds, unsigned int Size, int TimeTicks, const char *pTimestamp)
{
	CConsole *pCon = (CConsole *)GameServer()->Console();
	if(pCon->Cheated())
		return;
	for(unsigned int i = 0; i < Size; i++)
	{
		if(GameServer()->m_apPlayers[pClientIds[i]]->m_NotEligibleForFinish)
			return;
	}

	GameServer()->TeehistorianRecordTeamFinish(Team, TimeTicks);

	auto Tmp = std::make_unique<CSqlTeamScoreData>();
	for(unsigned int i = 0; i < Size; i++)
		str_copy(Tmp->m_aaNames[i], Server()->ClientName(pClientIds[i]), sizeof(Tmp->m_aaNames[i]));
	Tmp->m_Size = Size;
	Tmp->m_Time = (float)TimeTicks / (float)Server()->TickSpeed();
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));
	FormatUuid(GameServer()->GameUuid(), Tmp->m_aGameUuid, sizeof(Tmp->m_aGameUuid));
	str_copy(Tmp->m_aMap, Server()->GetMapName(), sizeof(Tmp->m_aMap));
	Tmp->m_TeamrankUuid = RandomUuid();

	m_pPool->ExecuteWrite(CScoreWorker::SaveTeamScore, std::move(Tmp), "save team score");
}

void CScore::ShowRank(int ClientId, const char *pName)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::ShowRank, "show rank", ClientId, pName, 0);
}

void CScore::ShowTeamRank(int ClientId, const char *pName)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::ShowTeamRank, "show team rank", ClientId, pName, 0);
}

void CScore::ShowTop(int ClientId, int Offset)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::ShowTop, "show top5", ClientId, "", Offset);
}

void CScore::ShowTeamTop5(int ClientId, int Offset)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::ShowTeamTop5, "show team top5", ClientId, "", Offset);
}

void CScore::ShowPlayerTeamTop5(int ClientId, const char *pName, int Offset)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::ShowPlayerTeamTop5, "show team top5 player", ClientId, pName, Offset);
}

void CScore::ShowTimes(int ClientId, int Offset)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::ShowTimes, "show times", ClientId, "", Offset);
}

void CScore::ShowTimes(int ClientId, const char *pName, int Offset)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::ShowTimes, "show times", ClientId, pName, Offset);
}

void CScore::ShowPoints(int ClientId, const char *pName)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::ShowPoints, "show points", ClientId, pName, 0);
}

void CScore::ShowTopPoints(int ClientId, int Offset)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::ShowTopPoints, "show top points", ClientId, "", Offset);
}

void CScore::RandomMap(int ClientId, int Stars)
{
	auto pResult = std::make_shared<CScoreRandomMapResult>(ClientId);
	GameServer()->m_SqlRandomMapResult = pResult;

	auto Tmp = std::make_unique<CSqlRandomMapRequest>(pResult);
	Tmp->m_Stars = Stars;
	str_copy(Tmp->m_aCurrentMap, Server()->GetMapName(), sizeof(Tmp->m_aCurrentMap));
	str_copy(Tmp->m_aServerType, g_Config.m_SvServerType, sizeof(Tmp->m_aServerType));
	str_copy(Tmp->m_aRequestingPlayer, ClientId == -1 ? "nameless tee" : GameServer()->Server()->ClientName(ClientId), sizeof(Tmp->m_aRequestingPlayer));

	m_pPool->Execute(CScoreWorker::RandomMap, std::move(Tmp), "random map");
}

void CScore::RandomUnfinishedMap(int ClientId, int Stars)
{
	auto pResult = std::make_shared<CScoreRandomMapResult>(ClientId);
	GameServer()->m_SqlRandomMapResult = pResult;

	auto Tmp = std::make_unique<CSqlRandomMapRequest>(pResult);
	Tmp->m_Stars = Stars;
	str_copy(Tmp->m_aCurrentMap, Server()->GetMapName(), sizeof(Tmp->m_aCurrentMap));
	str_copy(Tmp->m_aServerType, g_Config.m_SvServerType, sizeof(Tmp->m_aServerType));
	str_copy(Tmp->m_aRequestingPlayer, ClientId == -1 ? "nameless tee" : GameServer()->Server()->ClientName(ClientId), sizeof(Tmp->m_aRequestingPlayer));

	m_pPool->Execute(CScoreWorker::RandomUnfinishedMap, std::move(Tmp), "random unfinished map");
}

void CScore::SaveTeam(int ClientId, const char *pCode, const char *pServer)
{
	if(RateLimitPlayer(ClientId))
		return;
	auto *pController = GameServer()->m_pController;
	int Team = pController->Teams().m_Core.Team(ClientId);
	char aBuf[512];
	if(pController->Teams().GetSaving(Team))
	{
		GameServer()->SendChatTarget(ClientId, "Team save already in progress");
		return;
	}
	if(pController->Teams().IsPractice(Team))
	{
		GameServer()->SendChatTarget(ClientId, "Team save disabled for teams in practice mode");
		return;
	}

	auto SaveResult = std::make_shared<CScoreSaveResult>(ClientId);
	SaveResult->m_SaveId = RandomUuid();
	ESaveResult Result = SaveResult->m_SavedTeam.Save(GameServer(), Team);
	if(CSaveTeam::HandleSaveError(Result, ClientId, GameServer()))
		return;
	pController->Teams().SetSaving(Team, SaveResult);

	auto Tmp = std::make_unique<CSqlTeamSaveData>(SaveResult);
	str_copy(Tmp->m_aCode, pCode, sizeof(Tmp->m_aCode));
	str_copy(Tmp->m_aMap, Server()->GetMapName(), sizeof(Tmp->m_aMap));
	str_copy(Tmp->m_aServer, pServer, sizeof(Tmp->m_aServer));
	str_copy(Tmp->m_aClientName, this->Server()->ClientName(ClientId), sizeof(Tmp->m_aClientName));
	Tmp->m_aGeneratedCode[0] = '\0';
	GeneratePassphrase(Tmp->m_aGeneratedCode, sizeof(Tmp->m_aGeneratedCode));

	if(Tmp->m_aCode[0] == '\0')
	{
		str_format(aBuf,
			sizeof(aBuf),
			"Team save in progress. You'll be able to load with '/load %s'",
			Tmp->m_aGeneratedCode);
	}
	else
	{
		str_format(aBuf,
			sizeof(aBuf),
			"Team save in progress. You'll be able to load with '/load %s' if save is successful or with '/load %s' if it fails",
			Tmp->m_aCode,
			Tmp->m_aGeneratedCode);
	}
	pController->Teams().KillSavedTeam(ClientId, Team);
	GameServer()->SendChatTeam(Team, aBuf);
	m_pPool->ExecuteWrite(CScoreWorker::SaveTeam, std::move(Tmp), "save team");
}

void CScore::LoadTeam(const char *pCode, int ClientId)
{
	if(RateLimitPlayer(ClientId))
		return;
	auto *pController = GameServer()->m_pController;
	int Team = pController->Teams().m_Core.Team(ClientId);
	if(pController->Teams().GetSaving(Team))
	{
		GameServer()->SendChatTarget(ClientId, "Team load already in progress");
		return;
	}
	if(Team < TEAM_FLOCK || Team >= MAX_CLIENTS || (g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team == TEAM_FLOCK))
	{
		GameServer()->SendChatTarget(ClientId, "You have to be in a team (from 1-63)");
		return;
	}
	if(pController->Teams().GetTeamState(Team) != CGameTeams::TEAMSTATE_OPEN)
	{
		GameServer()->SendChatTarget(ClientId, "Team can't be loaded while racing");
		return;
	}
	if(pController->Teams().TeamFlock(Team))
	{
		GameServer()->SendChatTarget(ClientId, "Team can't be loaded while in team 0 mode");
		return;
	}
	if(pController->Teams().IsPractice(Team))
	{
		GameServer()->SendChatTarget(ClientId, "Team can't be loaded while practice is enabled");
		return;
	}
	auto SaveResult = std::make_shared<CScoreSaveResult>(ClientId);
	SaveResult->m_Status = CScoreSaveResult::LOAD_FAILED;
	pController->Teams().SetSaving(Team, SaveResult);
	auto Tmp = std::make_unique<CSqlTeamLoadRequest>(SaveResult);
	str_copy(Tmp->m_aCode, pCode, sizeof(Tmp->m_aCode));
	str_copy(Tmp->m_aMap, Server()->GetMapName(), sizeof(Tmp->m_aMap));
	str_copy(Tmp->m_aRequestingPlayer, Server()->ClientName(ClientId), sizeof(Tmp->m_aRequestingPlayer));
	Tmp->m_NumPlayer = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pController->Teams().m_Core.Team(i) == Team)
		{
			// put all names at the beginning of the array
			str_copy(Tmp->m_aClientNames[Tmp->m_NumPlayer], Server()->ClientName(i), sizeof(Tmp->m_aClientNames[Tmp->m_NumPlayer]));
			Tmp->m_aClientId[Tmp->m_NumPlayer] = i;
			Tmp->m_NumPlayer++;
		}
	}
	m_pPool->ExecuteWrite(CScoreWorker::LoadTeam, std::move(Tmp), "load team");
}

void CScore::GetSaves(int ClientId)
{
	if(RateLimitPlayer(ClientId))
		return;
	ExecPlayerThread(CScoreWorker::GetSaves, "get saves", ClientId, "", 0);
}
