/* CSqlScore class by Sushi */
#if defined(CONF_SQL)
#include <string.h>

#include <engine/shared/config.h>
#include "../entities/character.h"
#include "../gamemodes/DDRace.h"
#include "sql_score.h"
#include <engine/shared/console.h>

static LOCK gs_SqlLock = 0;

CSqlScore::CSqlScore(CGameContext *pGameServer) : m_pGameServer(pGameServer),
	m_pServer(pGameServer->Server()),
	m_pDatabase(g_Config.m_SvSqlDatabase),
	m_pPrefix(g_Config.m_SvSqlPrefix),
	m_pUser(g_Config.m_SvSqlUser),
	m_pPass(g_Config.m_SvSqlPw),
	m_pIp(g_Config.m_SvSqlIp),
	m_Port(g_Config.m_SvSqlPort)
{
	str_copy(m_aMap, g_Config.m_SvMap, sizeof(m_aMap));
	NormalizeMapname(m_aMap);

	if(gs_SqlLock == 0)
		gs_SqlLock = lock_create();

	Init();
}

CSqlScore::~CSqlScore()
{
	lock_wait(gs_SqlLock);
	lock_release(gs_SqlLock);
}

bool CSqlScore::Connect()
{
	try
	{
		// Create connection
		m_pDriver = get_driver_instance();
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "tcp://%s:%d", m_pIp, m_Port);
		m_pConnection = m_pDriver->connect(aBuf, m_pUser, m_pPass);

		// Create Statement
		m_pStatement = m_pConnection->createStatement();

		// Create database if not exists
		str_format(aBuf, sizeof(aBuf), "CREATE DATABASE IF NOT EXISTS %s", m_pDatabase);
		m_pStatement->execute(aBuf);

		// Connect to specific database
		m_pConnection->setSchema(m_pDatabase);
		dbg_msg("SQL", "SQL connection established");
		return true;
	}
	catch (sql::SQLException &e)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
		dbg_msg("SQL", aBuf);

		dbg_msg("SQL", "ERROR: SQL connection failed");
		return false;
	}
	catch (const std::exception& ex) {
		// ...
		dbg_msg("SQL", "1 %s",ex.what());

	} catch (const std::string& ex) {
		// ...
		dbg_msg("SQL", "2 %s",ex.c_str());
	}
	catch( int )
	{
	dbg_msg("SQL", "3 %s");
	}
	catch( float )
	{
	dbg_msg("SQL", "4 %s");
	}

	catch( char[] )
	{
	dbg_msg("SQL", "5 %s");
	}

	catch( char )
	{
	dbg_msg("SQL", "6 %s");
	}
	catch (...)
	{
		dbg_msg("SQL", "Unknown Error cause by the MySQL/C++ Connector, my advice compile server_debug and use it");

		dbg_msg("SQL", "ERROR: SQL connection failed");
		return false;
	}
	return false;
}

void CSqlScore::Disconnect()
{
	try
	{
		delete m_pConnection;
		dbg_msg("SQL", "SQL connection disconnected");
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("SQL", "ERROR: No SQL connection");
	}
}

// create tables... should be done only once
void CSqlScore::Init()
{
	// create connection
	if(Connect())
	{
		try
		{
			// create tables
			char aBuf[768];
			
			str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_%s_race (Name VARCHAR(%d) NOT NULL, Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP , Time FLOAT DEFAULT 0, cp1 FLOAT DEFAULT 0, cp2 FLOAT DEFAULT 0, cp3 FLOAT DEFAULT 0, cp4 FLOAT DEFAULT 0, cp5 FLOAT DEFAULT 0, cp6 FLOAT DEFAULT 0, cp7 FLOAT DEFAULT 0, cp8 FLOAT DEFAULT 0, cp9 FLOAT DEFAULT 0, cp10 FLOAT DEFAULT 0, cp11 FLOAT DEFAULT 0, cp12 FLOAT DEFAULT 0, cp13 FLOAT DEFAULT 0, cp14 FLOAT DEFAULT 0, cp15 FLOAT DEFAULT 0, cp16 FLOAT DEFAULT 0, cp17 FLOAT DEFAULT 0, cp18 FLOAT DEFAULT 0, cp19 FLOAT DEFAULT 0, cp20 FLOAT DEFAULT 0, cp21 FLOAT DEFAULT 0, cp22 FLOAT DEFAULT 0, cp23 FLOAT DEFAULT 0, cp24 FLOAT DEFAULT 0, cp25 FLOAT DEFAULT 0, KEY Name (Name)) CHARACTER SET utf8 ;", m_pPrefix, m_aMap, MAX_NAME_LENGTH);
			m_pStatement->execute(aBuf);

			// Check if table has new column with timestamp
			str_format(aBuf, sizeof(aBuf), "SELECT column_name FROM INFORMATION_SCHEMA.COLUMNS WHERE table_name = '%s_%s_race' AND column_name = 'Timestamp'",m_pPrefix, m_aMap);
			m_pResults = m_pStatement->executeQuery(aBuf);

			if(m_pResults->rowsCount() < 1){
				// If not... add the column				
				str_format(aBuf, sizeof(aBuf), "ALTER TABLE %s_%s_race ADD Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP AFTER Name, ADD INDEX(Name);",m_pPrefix, m_aMap);
				m_pStatement->execute(aBuf);
			}
			
			dbg_msg("SQL", "Tables were created successfully");

			// get the best time
			str_format(aBuf, sizeof(aBuf), "SELECT Time FROM %s_%s_race ORDER BY `Time` ASC LIMIT 0, 1;", m_pPrefix, m_aMap);
			m_pResults = m_pStatement->executeQuery(aBuf);

			if(m_pResults->next())
			{
				((CGameControllerDDRace*)GameServer()->m_pController)->m_CurrentRecord = (float)m_pResults->getDouble("Time");

				dbg_msg("SQL", "Getting best time on server done");

				// delete results
				delete m_pResults;
			}

			// delete statement
			delete m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Tables were NOT created");
		}

		// disconnect from database
		Disconnect();
	}
}

// update stuff
void CSqlScore::LoadScoreThread(void *pUser)
{
	lock_wait(gs_SqlLock);

	CSqlScoreData *pData = (CSqlScoreData *)pUser;

	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			// check strings
			pData->m_pSqlData->ClearString(pData->m_aName);

			char aBuf[512];


			str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_%s_race WHERE Name='%s' ORDER BY time ASC LIMIT 1;", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aName);
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);
			if(pData->m_pSqlData->m_pResults->next())
			{
				// get the best time
				pData->m_pSqlData->PlayerData(pData->m_ClientID)->m_BestTime = (float)pData->m_pSqlData->m_pResults->getDouble("Time");
				char aColumn[8];
				if(g_Config.m_SvCheckpointSave)
				{
					for(int i = 0; i < NUM_CHECKPOINTS; i++)
					{
						str_format(aColumn, sizeof(aColumn), "cp%d", i+1);
						pData->m_pSqlData->PlayerData(pData->m_ClientID)->m_aBestCpTime[i] = (float)pData->m_pSqlData->m_pResults->getDouble(aColumn);
					}
				}
			}

			dbg_msg("SQL", "Getting best time done");

			// delete statement and results
			delete pData->m_pSqlData->m_pStatement;
			delete pData->m_pSqlData->m_pResults;
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Could not update account");
		}

		// disconnect from database
		pData->m_pSqlData->Disconnect();
	}

	delete pData;

	lock_release(gs_SqlLock);
}

void CSqlScore::LoadScore(int ClientID)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), sizeof(Tmp->m_aName));
	Tmp->m_pSqlData = this;

	void *LoadThread = thread_create(LoadScoreThread, Tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)LoadThread);
#endif
}

void CSqlScore::SaveScoreThread(void *pUser)
{
	lock_wait(gs_SqlLock);

	CSqlScoreData *pData = (CSqlScoreData *)pUser;

	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			char aBuf[768];

			// check strings
			pData->m_pSqlData->ClearString(pData->m_aName);

			// if no entry found... create a new one
			str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_%s_race(Name, Timestamp, Time, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25) VALUES ('%s', CURRENT_TIMESTAMP(), '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f');", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aName, pData->m_Time, pData->m_aCpCurrent[0], pData->m_aCpCurrent[1], pData->m_aCpCurrent[2], pData->m_aCpCurrent[3], pData->m_aCpCurrent[4], pData->m_aCpCurrent[5], pData->m_aCpCurrent[6], pData->m_aCpCurrent[7], pData->m_aCpCurrent[8], pData->m_aCpCurrent[9], pData->m_aCpCurrent[10], pData->m_aCpCurrent[11], pData->m_aCpCurrent[12], pData->m_aCpCurrent[13], pData->m_aCpCurrent[14], pData->m_aCpCurrent[15], pData->m_aCpCurrent[16], pData->m_aCpCurrent[17], pData->m_aCpCurrent[18], pData->m_aCpCurrent[19], pData->m_aCpCurrent[20], pData->m_aCpCurrent[21], pData->m_aCpCurrent[22], pData->m_aCpCurrent[23], pData->m_aCpCurrent[24]);
			pData->m_pSqlData->m_pStatement->execute(aBuf);

			dbg_msg("SQL", "Updating time done");

			// delete results statement
			delete pData->m_pSqlData->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Could not update time");
		}

		// disconnect from database
		pData->m_pSqlData->Disconnect(); //TODO:Check if an exception is caught will this still execute ?
	}

	delete pData;

	lock_release(gs_SqlLock);
}

void CSqlScore::SaveScore(int ClientID, float Time, float CpTime[NUM_CHECKPOINTS])
{
	CConsole* pCon = (CConsole*)GameServer()->Console();
	if(pCon->m_Cheated)
		return;
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), sizeof(Tmp->m_aName));
	Tmp->m_Time = Time;
	for(int i = 0; i < NUM_CHECKPOINTS; i++)
		Tmp->m_aCpCurrent[i] = CpTime[i];
	Tmp->m_pSqlData = this;

	void *SaveThread = thread_create(SaveScoreThread, Tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)SaveThread);
#endif
}

void CSqlScore::ShowRankThread(void *pUser)
{
	lock_wait(gs_SqlLock);

	CSqlScoreData *pData = (CSqlScoreData *)pUser;

	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			// check strings
			char originalName[MAX_NAME_LENGTH];
			strcpy(originalName,pData->m_aName);
			pData->m_pSqlData->ClearString(pData->m_aName);

			// check sort methode
			char aBuf[600];

			pData->m_pSqlData->m_pStatement->execute("SET @rownum := 0;");
			str_format(aBuf, sizeof(aBuf), 	"SELECT Rank, one_rank.Name, one_rank.Time, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(r.Timestamp) as Ago, UNIX_TIMESTAMP(r.Timestamp) as stamp "
											"FROM ("
												"SELECT * FROM ("
													"SELECT @rownum := @rownum + 1 AS RANK, Name, Time "
													"FROM ("
														"SELECT Name, min(Time) as Time "
														"FROM %s_%s_race "
														"Group By Name) as all_top_times "
													"ORDER BY Time ASC) as all_ranks "
												"WHERE all_ranks.Name = '%s') as one_rank "
											"LEFT JOIN %s_%s_race as r "
											"ON one_rank.Name = r.Name && one_rank.Time = r.Time "
											"ORDER BY Ago ASC "
											"LIMIT 0,1"
											";", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap,pData->m_aName, pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap);

			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);

			if(pData->m_pSqlData->m_pResults->rowsCount() != 1)
			{
				str_format(aBuf, sizeof(aBuf), "%s is not ranked", originalName);
				pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
			}
			else
			{
				pData->m_pSqlData->m_pResults->next();
				int since = (int)pData->m_pSqlData->m_pResults->getInt("Ago");
				char agoString[40];
				agoTimeToString(since,agoString);

				float Time = (float)pData->m_pSqlData->m_pResults->getDouble("Time");
				int Rank = (int)pData->m_pSqlData->m_pResults->getInt("Rank");
				if(g_Config.m_SvHideScore)
					str_format(aBuf, sizeof(aBuf), "Your time: %d minute(s) %5.2f second(s)", (int)(Time/60), Time-((int)Time/60*60));
				else
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %5.2f second(s)", Rank, pData->m_pSqlData->m_pResults->getString("Name").c_str(), (int)(Time/60), Time-((int)Time/60*60), agoString);

				if(pData->m_pSqlData->m_pResults->getInt("stamp") != 0){
					pData->m_pSqlData->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, pData->m_ClientID);
					str_format(aBuf, sizeof(aBuf), "Finished: %s ago", agoString);
				}
				if(pData->m_Search)
					strcat(aBuf, pData->m_aRequestingPlayer);
				pData->m_pSqlData->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, pData->m_ClientID);

			}

			dbg_msg("SQL", "Showing rank done");

			// delete results and statement
			delete pData->m_pSqlData->m_pResults;
			delete pData->m_pSqlData->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Could not show rank");
		}

		// disconnect from database
		pData->m_pSqlData->Disconnect();//TODO:Check if an exception is caught will this still execute ?
	}

	delete pData;

	lock_release(gs_SqlLock);
}

void CSqlScore::ShowRank(int ClientID, const char* pName, bool Search)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, pName, sizeof(Tmp->m_aName));
	Tmp->m_Search = Search;
	str_format(Tmp->m_aRequestingPlayer, sizeof(Tmp->m_aRequestingPlayer), " (%s)", Server()->ClientName(ClientID));
	Tmp->m_pSqlData = this;

	void *RankThread = thread_create(ShowRankThread, Tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)RankThread);
#endif
}

void CSqlScore::ShowTop5Thread(void *pUser)
{
	lock_wait(gs_SqlLock);

	CSqlScoreData *pData = (CSqlScoreData *)pUser;

	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			// check sort methode
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SELECT Name, min(Time) as Time FROM %s_%s_race Group By Name ORDER BY `Time` ASC LIMIT %d, 5;", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_Num-1);
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);

			// show top5
			pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, "----------- Top 5 -----------");

			int Rank = pData->m_Num;
			float Time = 0;
			while(pData->m_pSqlData->m_pResults->next())
			{
				Time = (float)pData->m_pSqlData->m_pResults->getDouble("Time");
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %d minute(s) %.2f second(s)", Rank, pData->m_pSqlData->m_pResults->getString("Name").c_str(), (int)(Time/60), Time-((int)Time/60*60));
				pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
				Rank++;
			}
			pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, "-------------------------------");

			dbg_msg("SQL", "Showing top5 done");

			// delete results and statement
			delete pData->m_pSqlData->m_pResults;
			delete pData->m_pSqlData->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Could not show top5");
		}

		// disconnect from database
		pData->m_pSqlData->Disconnect();
	}

	delete pData;

	lock_release(gs_SqlLock);
}

void CSqlScore::ShowTimesThread(void *pUser)
{
	lock_wait(gs_SqlLock);
	CSqlScoreData *pData = (CSqlScoreData *)pUser;

	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			char originalName[MAX_NAME_LENGTH];
			strcpy(originalName,pData->m_aName);
			pData->m_pSqlData->ClearString(pData->m_aName);
			
			char aBuf[512];

			if(pData->m_Search) // last 5 times of a player
				str_format(aBuf, sizeof(aBuf), "SELECT Time, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago, UNIX_TIMESTAMP(Timestamp) as Stamp FROM %s_%s_race WHERE Name = '%s' ORDER BY Ago ASC LIMIT %d, 5;", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_aName, pData->m_Num-1);
			else // last 5 times of server
				str_format(aBuf, sizeof(aBuf), "SELECT Name, Time, UNIX_TIMESTAMP(CURRENT_TIMESTAMP)-UNIX_TIMESTAMP(Timestamp) as Ago, UNIX_TIMESTAMP(Timestamp) as Stamp FROM %s_%s_race ORDER BY Ago ASC LIMIT %d, 5;", pData->m_pSqlData->m_pPrefix, pData->m_pSqlData->m_aMap, pData->m_Num-1);

			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);

			// show top5
			if(pData->m_pSqlData->m_pResults->rowsCount() == 0){
				pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, "There are no times in the specified range");				
				goto end;
			}
			
			str_format(aBuf, sizeof(aBuf), "------------ Last Times No %d - %d ------------",pData->m_Num,pData->m_Num + pData->m_pSqlData->m_pResults->rowsCount() - 1);
			pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);

			float pTime = 0;
			int pSince = 0;
			int pStamp = 0;

			while(pData->m_pSqlData->m_pResults->next())
			{
				char pAgoString[40] = "\0";
				pSince = (int)pData->m_pSqlData->m_pResults->getInt("Ago");
				pStamp = (int)pData->m_pSqlData->m_pResults->getInt("Stamp");
				pTime = (float)pData->m_pSqlData->m_pResults->getDouble("Time");

				agoTimeToString(pSince,pAgoString);								
				
				if(pData->m_Search) // last 5 times of a player
				{
					if(pStamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
						str_format(aBuf, sizeof(aBuf), "%d min %.2f sec, don't know how long ago", (int)(pTime/60), pTime-((int)pTime/60*60));
					else					
						str_format(aBuf, sizeof(aBuf), "%s ago, %d min %.2f sec", pAgoString,(int)(pTime/60), pTime-((int)pTime/60*60));
				}
				else // last 5 times of the server
				{
					if(pStamp == 0) // stamp is 00:00:00 cause it's an old entry from old times where there where no stamps yet
						str_format(aBuf, sizeof(aBuf), "%s, %d m %.2f s, don't know when", pData->m_pSqlData->m_pResults->getString("Name").c_str(), (int)(pTime/60), pTime-((int)pTime/60*60));
					else					
						str_format(aBuf, sizeof(aBuf), "%s, %s ago, %d m %.2f s", pData->m_pSqlData->m_pResults->getString("Name").c_str(), pAgoString, (int)(pTime/60), pTime-((int)pTime/60*60));
				}
				pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf);
			}
			pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, "----------------------------------------------------");

			dbg_msg("SQL", "Showing times done");

			// delete results and statement
			delete pData->m_pSqlData->m_pResults;
			delete pData->m_pSqlData->m_pStatement;
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Could not show times");
		}
end:
		// disconnect from database
		pData->m_pSqlData->Disconnect();
	}
	delete pData;

	lock_release(gs_SqlLock);
}

void CSqlScore::ShowTop5(IConsole::IResult *pResult, int ClientID, void *pUserData, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;
	Tmp->m_pSqlData = this;

	void *Top5Thread = thread_create(ShowTop5Thread, Tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)Top5Thread);
#endif
}


void CSqlScore::ShowTimes(int ClientID, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;
	Tmp->m_pSqlData = this;
	Tmp->m_Search = false;

	void *TimesThread = thread_create(ShowTimesThread, Tmp);
	#if defined(CONF_FAMILY_UNIX)
		pthread_detach((pthread_t)TimesThread);
	#endif	
}

void CSqlScore::ShowTimes(int ClientID, const char* pName, int Debut)
{
	CSqlScoreData *Tmp = new CSqlScoreData();
	Tmp->m_Num = Debut;
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, pName, sizeof(Tmp->m_aName));
	Tmp->m_pSqlData = this;
	Tmp->m_Search = true;
	
	void *TimesThread = thread_create(ShowTimesThread, Tmp);
	#if defined(CONF_FAMILY_UNIX)
		pthread_detach((pthread_t)TimesThread);
	#endif	
}

// anti SQL injection

void CSqlScore::ClearString(char *pString)
{
	char newString[MAX_NAME_LENGTH*2-1];
	int pos = 0;

	for(int i=0;i<str_length(pString);i++) {
		if(pString[i] == '\\') {
			newString[pos++] = '\\';
			newString[pos++] = '\\';
		} else if(pString[i] == '\'') {
			newString[pos++] = '\\';
			newString[pos++] = '\'';
		} else if(pString[i] == '"') {
			newString[pos++] = '\\';
			newString[pos++] = '"';
		} else {
			newString[pos++] = pString[i];
		}
	}

	newString[pos] = '\0';

	strcpy(pString,newString);
}

void CSqlScore::NormalizeMapname(char *pString) {
	std::string validChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");

	for(int i=0;i<str_length(pString);i++) {
		if(validChars.find(pString[i]) == std::string::npos) {
			pString[i] = '_';
		}
	}
}

void CSqlScore::agoTimeToString(int agoTime, char agoString[]){
	char aBuf[20];
	int times[7] = {
		60 * 60 * 24 * 365 ,
		60 * 60 * 24 * 30 ,
		60 * 60 * 24 * 7,
		60 * 60 * 24 ,
		60 * 60 ,
		60 ,
		1
	};
	char names[7][6] = {
		"year",
		"month",
		"week",
		"day",
		"hour",
		"min",
		"sec"
	};

	int seconds = 0;
	char name[6];
	int count = 0;
	int i = 0;

	// finding biggest match
	for(i = 0; i<7; i++){
		seconds = times[i];
		strcpy(name,names[i]);

		count = floor((float)agoTime/(float)seconds);
		if(count != 0){
			break;
		}
	}

	if(count == 1){
		str_format(aBuf, sizeof(aBuf), "%d %s", 1 , name);
	}else{
		str_format(aBuf, sizeof(aBuf), "%d %ss", count , name);
	}
	strcat(agoString,aBuf);

	if (i + 1 < 7) {
		// getting second piece now
		int seconds2 = times[i+1];
		char name2[6];
		strcpy(name2,names[i+1]);

		// add second piece if it's greater than 0
		int count2 = floor((float)(agoTime - (seconds * count)) / (float)seconds2);

		if (count2 != 0) {
			if(count2 == 1){
				str_format(aBuf, sizeof(aBuf), " and %d %s", 1 , name2);
			}else{
				str_format(aBuf, sizeof(aBuf), " and %d %ss", count2 , name2);
			}
			strcat(agoString,aBuf);
		}
	}
}
#endif
