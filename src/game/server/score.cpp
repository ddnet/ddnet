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

std::shared_ptr<CScorePlayerResult> CScore::NewSqlPlayerResult(int ClientID)
{
	CPlayer *pCurPlayer = GameServer()->m_apPlayers[ClientID];
	if(pCurPlayer->m_ScoreQueryResult != nullptr) // TODO: send player a message: "too many requests"
		return nullptr;
	pCurPlayer->m_ScoreQueryResult = std::make_shared<CScorePlayerResult>();
	return pCurPlayer->m_ScoreQueryResult;
}

void CScore::ExecPlayerThread(
	bool (*pFuncPtr)(IDbConnection *, const ISqlData *, char *pError, int ErrorSize),
	const char *pThreadName,
	int ClientID,
	const char *pName,
	int Offset)
{
	auto pResult = NewSqlPlayerResult(ClientID);
	if(pResult == nullptr)
		return;
	auto Tmp = std::make_unique<CSqlPlayerRequest>(pResult);
	str_copy(Tmp->m_aName, pName, sizeof(Tmp->m_aName));
	str_copy(Tmp->m_aMap, g_Config.m_SvMap, sizeof(Tmp->m_aMap));
	str_copy(Tmp->m_aServer, g_Config.m_SvSqlServerName, sizeof(Tmp->m_aServer));
	str_copy(Tmp->m_aRequestingPlayer, Server()->ClientName(ClientID), sizeof(Tmp->m_aRequestingPlayer));
	Tmp->m_Offset = Offset;

	m_pPool->Execute(pFuncPtr, std::move(Tmp), pThreadName);
}

bool CScore::RateLimitPlayer(int ClientID)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if(pPlayer == 0)
		return true;
	if(pPlayer->m_LastSQLQuery + (int64_t)g_Config.m_SvSqlQueriesDelay * Server()->TickSpeed() >= Server()->Tick())
		return true;
	pPlayer->m_LastSQLQuery = Server()->Tick();
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

	IOHANDLE File = GameServer()->Storage()->OpenFile("wordlist.txt", IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_ALL);
	if(File)
	{
		CLineReader LineReader;
		LineReader.Init(File);
		char *pLine;
		while((pLine = LineReader.Get()))
		{
			char aWord[32] = {0};
			sscanf(pLine, "%*s %31s", aWord);
			aWord[31] = 0;
			m_vWordlist.emplace_back(aWord);
		}
		io_close(File);
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

	auto Tmp = std::make_unique<CSqlLoadBestTimeData>(LoadBestTimeResult);
	str_copy(Tmp->m_aMap, g_Config.m_SvMap, sizeof(Tmp->m_aMap));
	m_pPool->Execute(CScoreWorker::LoadBestTime, std::move(Tmp), "load best time");
}

void CScore::LoadPlayerData(int ClientID, const char *pName)
{
	ExecPlayerThread(CScoreWorker::LoadPlayerData, "load player data", ClientID, pName, 0);
}

void CScore::MapVote(int ClientID, const char *pMapName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::MapVote, "map vote", ClientID, pMapName, 0);
}

void CScore::MapInfo(int ClientID, const char *pMapName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::MapInfo, "map info", ClientID, pMapName, 0);
}

void CScore::SaveScore(int ClientID, float Time, const char *pTimestamp, const float aTimeCp[NUM_CHECKPOINTS], bool NotEligible)
{
	CConsole *pCon = (CConsole *)GameServer()->Console();
	if(pCon->Cheated() || NotEligible)
		return;

	CPlayer *pCurPlayer = GameServer()->m_apPlayers[ClientID];
	if(pCurPlayer->m_ScoreFinishResult != nullptr)
		dbg_msg("sql", "WARNING: previous save score result didn't complete, overwriting it now");
	pCurPlayer->m_ScoreFinishResult = std::make_shared<CScorePlayerResult>();
	auto Tmp = std::make_unique<CSqlScoreData>(pCurPlayer->m_ScoreFinishResult);
	str_copy(Tmp->m_aMap, g_Config.m_SvMap, sizeof(Tmp->m_aMap));
	FormatUuid(GameServer()->GameUuid(), Tmp->m_aGameUuid, sizeof(Tmp->m_aGameUuid));
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), sizeof(Tmp->m_aName));
	Tmp->m_Time = Time;
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
		Tmp->m_aCurrentTimeCp[i] = aTimeCp[i];

	m_pPool->ExecuteWrite(CScoreWorker::SaveScore, std::move(Tmp), "save score");
}

void CScore::SaveTeamScore(int *pClientIDs, unsigned int Size, float Time, const char *pTimestamp)
{
	CConsole *pCon = (CConsole *)GameServer()->Console();
	if(pCon->Cheated())
		return;
	for(unsigned int i = 0; i < Size; i++)
	{
		if(GameServer()->m_apPlayers[pClientIDs[i]]->m_NotEligibleForFinish)
			return;
	}
	auto Tmp = std::make_unique<CSqlTeamScoreData>();
	for(unsigned int i = 0; i < Size; i++)
		str_copy(Tmp->m_aaNames[i], Server()->ClientName(pClientIDs[i]), sizeof(Tmp->m_aaNames[i]));
	Tmp->m_Size = Size;
	Tmp->m_Time = Time;
	str_copy(Tmp->m_aTimestamp, pTimestamp, sizeof(Tmp->m_aTimestamp));
	FormatUuid(GameServer()->GameUuid(), Tmp->m_aGameUuid, sizeof(Tmp->m_aGameUuid));
	str_copy(Tmp->m_aMap, g_Config.m_SvMap, sizeof(Tmp->m_aMap));
	Tmp->m_TeamrankUuid = RandomUuid();

	m_pPool->ExecuteWrite(CScoreWorker::SaveTeamScore, std::move(Tmp), "save team score");
}

void CScore::ShowRank(int ClientID, const char *pName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::ShowRank, "show rank", ClientID, pName, 0);
}

void CScore::ShowTeamRank(int ClientID, const char *pName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::ShowTeamRank, "show team rank", ClientID, pName, 0);
}

void CScore::ShowTop(int ClientID, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::ShowTop, "show top5", ClientID, "", Offset);
}

void CScore::ShowTeamTop5(int ClientID, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::ShowTeamTop5, "show team top5", ClientID, "", Offset);
}

void CScore::ShowPlayerTeamTop5(int ClientID, const char *pName, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::ShowPlayerTeamTop5, "show team top5 player", ClientID, pName, Offset);
}

void CScore::ShowTimes(int ClientID, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::ShowTimes, "show times", ClientID, "", Offset);
}

void CScore::ShowTimes(int ClientID, const char *pName, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::ShowTimes, "show times", ClientID, pName, Offset);
}

void CScore::ShowPoints(int ClientID, const char *pName)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::ShowPoints, "show points", ClientID, pName, 0);
}

void CScore::ShowTopPoints(int ClientID, int Offset)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::ShowTopPoints, "show top points", ClientID, "", Offset);
}

void CScore::RandomMap(int ClientID, int Stars)
{
	auto pResult = std::make_shared<CScoreRandomMapResult>(ClientID);
	GameServer()->m_SqlRandomMapResult = pResult;

	auto Tmp = std::make_unique<CSqlRandomMapRequest>(pResult);
	Tmp->m_Stars = Stars;
	str_copy(Tmp->m_aCurrentMap, g_Config.m_SvMap, sizeof(Tmp->m_aCurrentMap));
	str_copy(Tmp->m_aServerType, g_Config.m_SvServerType, sizeof(Tmp->m_aServerType));
	str_copy(Tmp->m_aRequestingPlayer, GameServer()->Server()->ClientName(ClientID), sizeof(Tmp->m_aRequestingPlayer));

	m_pPool->Execute(CScoreWorker::RandomMap, std::move(Tmp), "random map");
}

void CScore::RandomUnfinishedMap(int ClientID, int Stars)
{
	auto pResult = std::make_shared<CScoreRandomMapResult>(ClientID);
	GameServer()->m_SqlRandomMapResult = pResult;

	auto Tmp = std::make_unique<CSqlRandomMapRequest>(pResult);
	Tmp->m_Stars = Stars;
	str_copy(Tmp->m_aCurrentMap, g_Config.m_SvMap, sizeof(Tmp->m_aCurrentMap));
	str_copy(Tmp->m_aServerType, g_Config.m_SvServerType, sizeof(Tmp->m_aServerType));
	str_copy(Tmp->m_aRequestingPlayer, GameServer()->Server()->ClientName(ClientID), sizeof(Tmp->m_aRequestingPlayer));

	m_pPool->Execute(CScoreWorker::RandomUnfinishedMap, std::move(Tmp), "random unfinished map");
}

void CScore::SaveTeam(int ClientID, const char *pCode, const char *pServer, bool Silent)
{
	if(RateLimitPlayer(ClientID))
		return;
	auto *pController = GameServer()->m_pController;
	int Team = pController->Teams().m_Core.Team(ClientID);
	if(pController->Teams().GetSaving(Team))
		return;

	auto SaveResult = std::make_shared<CScoreSaveResult>(ClientID);
	SaveResult->m_SaveID = RandomUuid();
	int Result = SaveResult->m_SavedTeam.Save(GameServer(), Team);
	if(Silent)
	{
		if(Result)
			return;
	}
	else if(CSaveTeam::HandleSaveError(Result, ClientID, GameServer()))
		return;
	pController->Teams().SetSaving(Team, SaveResult);

	auto Tmp = std::make_unique<CSqlTeamSave>(SaveResult);
	str_copy(Tmp->m_aCode, pCode, sizeof(Tmp->m_aCode));
	str_copy(Tmp->m_aMap, this->Server()->GetMapName(), sizeof(Tmp->m_aMap));
	str_copy(Tmp->m_aServer, pServer, sizeof(Tmp->m_aServer));
	str_copy(Tmp->m_aClientName, this->Server()->ClientName(ClientID), sizeof(Tmp->m_aClientName));
	Tmp->m_aGeneratedCode[0] = '\0';
	GeneratePassphrase(Tmp->m_aGeneratedCode, sizeof(Tmp->m_aGeneratedCode));

	char aBuf[512];
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
	pController->Teams().KillSavedTeam(ClientID, Team);
	GameServer()->SendChatTeam(Team, aBuf);
	m_pPool->ExecuteWrite(CScoreWorker::SaveTeam, std::move(Tmp), "save team");
}

void CScore::LoadTeam(const char *pCode, int ClientID)
{
	if(RateLimitPlayer(ClientID))
		return;
	auto *pController = GameServer()->m_pController;
	int Team = pController->Teams().m_Core.Team(ClientID);
	if(pController->Teams().GetSaving(Team))
		return;
	if(Team < TEAM_FLOCK || Team >= MAX_CLIENTS || (g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team == TEAM_FLOCK))
	{
		GameServer()->SendChatTarget(ClientID, "You have to be in a team (from 1-63)");
		return;
	}
	if(pController->Teams().GetTeamState(Team) != CGameTeams::TEAMSTATE_OPEN)
	{
		GameServer()->SendChatTarget(ClientID, "Team can't be loaded while racing");
		return;
	}
	auto SaveResult = std::make_shared<CScoreSaveResult>(ClientID);
	SaveResult->m_Status = CScoreSaveResult::LOAD_FAILED;
	pController->Teams().SetSaving(Team, SaveResult);
	auto Tmp = std::make_unique<CSqlTeamLoad>(SaveResult);
	str_copy(Tmp->m_aCode, pCode, sizeof(Tmp->m_aCode));
	str_copy(Tmp->m_aMap, g_Config.m_SvMap, sizeof(Tmp->m_aMap));
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aRequestingPlayer, Server()->ClientName(ClientID), sizeof(Tmp->m_aRequestingPlayer));
	Tmp->m_NumPlayer = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pController->Teams().m_Core.Team(i) == Team)
		{
			// put all names at the beginning of the array
			str_copy(Tmp->m_aClientNames[Tmp->m_NumPlayer], Server()->ClientName(i), sizeof(Tmp->m_aClientNames[Tmp->m_NumPlayer]));
			Tmp->m_aClientID[Tmp->m_NumPlayer] = i;
			Tmp->m_NumPlayer++;
		}
	}
	m_pPool->ExecuteWrite(CScoreWorker::LoadTeam, std::move(Tmp), "load team");
}

void CScore::GetSaves(int ClientID)
{
	if(RateLimitPlayer(ClientID))
		return;
	ExecPlayerThread(CScoreWorker::GetSaves, "get saves", ClientID, "", 0);
}
