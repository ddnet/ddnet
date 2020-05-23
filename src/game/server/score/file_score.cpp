/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* copyright (c) 2008 rajh and gregwar. Score stuff */
#include <base/tl/sorted_array.h>

#include <engine/shared/config.h>
#include <sstream>
#include <fstream>
#include "../gamemodes/DDRace.h"
#include "file_score.h"
#include <engine/shared/console.h>

static LOCK gs_ScoreLock = 0;

CFileScore::CPlayerScore::CPlayerScore(const char *pName, float Score,
		float aCpTime[NUM_CHECKPOINTS])
{
	str_copy(m_aName, pName, sizeof(m_aName));
	m_Score = Score;
	for (int i = 0; i < NUM_CHECKPOINTS; i++)
		m_aCpTime[i] = aCpTime[i];
}

CFileScore::CFileScore(CGameContext *pGameServer) :
				m_pGameServer(pGameServer), m_pServer(pGameServer->Server())
{
	if (gs_ScoreLock == 0)
		gs_ScoreLock = lock_create();

	Init();
}

CFileScore::~CFileScore()
{
	lock_wait(gs_ScoreLock);

	// clear list
	m_Top.clear();

	lock_unlock(gs_ScoreLock);
}

std::string CFileScore::SaveFile()
{
	std::ostringstream oss;
	char aBuf[256];
	str_copy(aBuf, Server()->GetMapName(), sizeof(aBuf));
	for(int i = 0; i < 256; i++) if(aBuf[i] == '/') aBuf[i] = '-';
	if (g_Config.m_SvScoreFolder[0])
		oss << g_Config.m_SvScoreFolder << "/" << aBuf << "_record.dtb";
	else
		oss << Server()->GetMapName() << "_record.dtb";
	return oss.str();
}

void CFileScore::MapInfo(int ClientID, const char* MapName)
{
	// TODO: implement
}

void CFileScore::MapVote(std::shared_ptr<CMapVoteResult> *ppResult, int ClientID, const char* MapName)
{
	// TODO: implement
}

void CFileScore::SaveScoreThread(void *pUser)
{
	CFileScore *pSelf = (CFileScore *) pUser;
	lock_wait(gs_ScoreLock);
	std::fstream f;
	f.open(pSelf->SaveFile().c_str(), std::ios::out);
	if(f.fail())
	{
		dbg_msg("filescore", "opening '%s' for writing failed", pSelf->SaveFile().c_str());
	}
	else
	{
		int t = 0;
		for (sorted_array<CPlayerScore>::range r = pSelf->m_Top.all();
				!r.empty(); r.pop_front())
		{
			f << r.front().m_aName << std::endl << r.front().m_Score
					<< std::endl;
			if (g_Config.m_SvCheckpointSave)
			{
				for (int c = 0; c < NUM_CHECKPOINTS; c++)
					f << r.front().m_aCpTime[c] << " ";
				f << std::endl;
			}
			t++;
			if (t % 50 == 0)
				thread_sleep(1000);
		}
	}
	f.close();
	lock_unlock(gs_ScoreLock);
}

void CFileScore::Save()
{
	thread_init_and_detach(SaveScoreThread, this, "FileScore save");
}

void CFileScore::Init()
{
	lock_wait(gs_ScoreLock);

	// create folder if not exist
	if (g_Config.m_SvScoreFolder[0])
		fs_makedir(g_Config.m_SvScoreFolder);

	std::fstream f;
	f.open(SaveFile().c_str(), std::ios::in);

	if(f.fail())
	{
		dbg_msg("filescore", "opening '%s' for reading failed", SaveFile().c_str());
	}
	while (!f.eof() && !f.fail())
	{
		std::string TmpName, TmpScore, TmpCpLine;
		std::getline(f, TmpName);
		if (!f.eof() && TmpName != "")
		{
			std::getline(f, TmpScore);
			float aTmpCpTime[NUM_CHECKPOINTS] =
			{ 0 };
			if (g_Config.m_SvCheckpointSave)
			{
				std::getline(f, TmpCpLine);

				std::istringstream iss(TmpCpLine);
				int i = 0;
				for(std::string p; std::getline(iss, p, ' '); i++)
					aTmpCpTime[i] = std::stof(p, NULL);
			}
			m_Top.add(
					*new CPlayerScore(TmpName.c_str(), atof(TmpScore.c_str()),
							aTmpCpTime));
		}
	}
	f.close();
	lock_unlock(gs_ScoreLock);

	// save the current best score
	if (m_Top.size())
		((CGameControllerDDRace*) GameServer()->m_pController)->m_CurrentRecord =
				m_Top[0].m_Score;
}

CFileScore::CPlayerScore *CFileScore::SearchName(const char *pName,
		int *pPosition, bool NoCase)
{
	CPlayerScore *pPlayer = 0;
	int Pos = 1;
	int Found = 0;
	for (sorted_array<CPlayerScore>::range r = m_Top.all(); !r.empty();
			r.pop_front())
	{
		if (str_find_nocase(r.front().m_aName, pName))
		{
			if (pPosition)
				*pPosition = Pos;
			if (NoCase)
			{
				Found++;
				pPlayer = &r.front();
			}
			if (!str_comp(r.front().m_aName, pName))
				return &r.front();
		}
		Pos++;
	}
	if (Found > 1)
	{
		if (pPosition)
			*pPosition = -1;
		return 0;
	}
	return pPlayer;
}

void CFileScore::UpdatePlayer(int ID, float Score,
		float aCpTime[NUM_CHECKPOINTS])
{
	const char *pName = Server()->ClientName(ID);

	lock_wait(gs_ScoreLock);
	CPlayerScore *pPlayer = SearchScore(ID, 0);

	if (pPlayer)
	{
		for (int c = 0; c < NUM_CHECKPOINTS; c++)
			pPlayer->m_aCpTime[c] = aCpTime[c];

		pPlayer->m_Score = Score;
		str_copy(pPlayer->m_aName, pName, sizeof(pPlayer->m_aName));

		sort(m_Top.all());
	}
	else
		m_Top.add(*new CPlayerScore(pName, Score, aCpTime));

	lock_unlock(gs_ScoreLock);
	Save();
}

void CFileScore::CheckBirthday(int ClientID)
{
	// TODO: implement
}

void CFileScore::LoadScore(int ClientID)
{
	CPlayerScore *pPlayer = SearchScore(ClientID, 0);
	if (pPlayer)
	{
		lock_wait(gs_ScoreLock);
		lock_unlock(gs_ScoreLock);
		Save();
	}

	// set score
	if (pPlayer)
	{
		PlayerData(ClientID)->Set(pPlayer->m_Score, pPlayer->m_aCpTime);
		GameServer()->m_apPlayers[ClientID]->m_HasFinishScore = true;
	}
}

void CFileScore::SaveTeamScore(int* ClientIDs, unsigned int Size, float Time, const char *pTimestamp)
{
	dbg_msg("filescore", "saveteamscore not implemented for filescore");
}

void CFileScore::SaveScore(int ClientID, float Time, const char *pTimestamp,
		float CpTime[NUM_CHECKPOINTS], bool NotEligible)
{
	CConsole* pCon = (CConsole*) GameServer()->Console();
	if (!pCon->m_Cheated || g_Config.m_SvRankCheats)
		UpdatePlayer(ClientID, Time, CpTime);
}

void CFileScore::ShowTop5(IConsole::IResult *pResult, int ClientID,
		void *pUserData, int Debut)
{
	CGameContext *pSelf = (CGameContext *) pUserData;
	char aBuf[512];
	Debut = maximum(1, Debut < 0 ? m_Top.size() + Debut - 3 : Debut);
	pSelf->SendChatTarget(ClientID, "----------- Top 5 -----------");
	for (int i = 0; i < 5; i++)
	{
		if (i + Debut > m_Top.size())
			break;
		CPlayerScore *r = &m_Top[i + Debut - 1];
		str_format(aBuf, sizeof(aBuf),
				"%d. %s Time: %d minute(s) %5.2f second(s)", i + Debut,
				r->m_aName, (int)r->m_Score / 60,
				r->m_Score - ((int)r->m_Score / 60 * 60));
		pSelf->SendChatTarget(ClientID, aBuf);
	}
	pSelf->SendChatTarget(ClientID, "------------------------------");
}

void CFileScore::ShowRank(int ClientID, const char* pName, bool Search)
{
	CPlayerScore *pScore;
	int Pos = -2;
	char aBuf[512];

	if (!Search)
		pScore = SearchScore(ClientID, &Pos);
	else
		pScore = SearchName(pName, &Pos, 1);

	if (pScore && Pos > -1)
	{
		float Time = pScore->m_Score;
		if (g_Config.m_SvHideScore)
			str_format(aBuf, sizeof(aBuf),
					"Your time: %d minute(s) %5.2f second(s)", (int)Time / 60,
					Time - ((int)Time / 60 * 60));
		else
			str_format(aBuf, sizeof(aBuf),
					"%d. %s Time: %d minute(s) %5.2f second(s), requested by (%s)", Pos,
					pScore->m_aName, (int)Time / 60,
					Time - ((int)Time / 60 * 60), Server()->ClientName(ClientID));
		if (!Search)
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, ClientID);
		else
			GameServer()->SendChatTarget(ClientID, aBuf);
		return;
	}
	else if (Pos == -1)
		str_format(aBuf, sizeof(aBuf), "Several players were found.");
	else
		str_format(aBuf, sizeof(aBuf), "%s is not ranked",
				Search ? pName : Server()->ClientName(ClientID));

	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CFileScore::ShowTeamTop5(IConsole::IResult *pResult, int ClientID, void *pUserData, int Debut)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Team ranks not supported in file based servers");
	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CFileScore::ShowTeamRank(int ClientID, const char* pName, bool Search)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Team ranks not supported in file based servers");
	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CFileScore::ShowTopPoints(IConsole::IResult *pResult, int ClientID, void *pUserData, int Debut)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Team ranks not supported in file based servers");
	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CFileScore::ShowPoints(int ClientID, const char* pName, bool Search)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Points not supported in file based servers");
	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CFileScore::RandomMap(std::shared_ptr<CRandomMapResult> *ppResult, int ClientID, int stars)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Random map not supported in file based servers");
	GameServer()->SendChatTarget(ClientID, aBuf);
	*ppResult = NULL;
}

void CFileScore::RandomUnfinishedMap(std::shared_ptr<CRandomMapResult> *ppResult, int ClientID, int stars)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Random unfinished map not supported in file based servers");
	GameServer()->SendChatTarget(ClientID, aBuf);
	*ppResult = NULL;
}

void CFileScore::SaveTeam(int Team, const char* Code, int ClientID, const char* Server)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Save-function not supported in file based servers");
	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CFileScore::LoadTeam(const char* Code, int ClientID)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Save-function not supported in file based servers");
	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CFileScore::GetSaves(int ClientID)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Save-function not supported in file based servers");
	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CFileScore::OnShutdown()
{
	;
}
