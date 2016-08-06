#if defined(CONF_SQL)

#include <cstring>

#include <engine/external/json/json.h>
#include <engine/shared/linereader.h>

#include "restore_ranks.h"

using json = nlohmann::json;


CRestoreRanks::CRestoreRanks(IConsole* pConsole, IConfig* pConfig) :
m_pConsole(pConsole),
m_pConfig(pConfig),
m_ParseFailure(false)
{
	str_format(m_FailureFileTmp, sizeof(m_FailureFileTmp), "%s.tmp", g_Config.m_SvSqlFailureFile);
	dbg_msg("restore_ranks", "moving '%s' to '%s'", g_Config.m_SvSqlFailureFile, m_FailureFileTmp);
	if (fs_rename(g_Config.m_SvSqlFailureFile, m_FailureFileTmp))
	{
		dbg_msg("restore_ranks", "failed to move file");
		m_ParseFailure = true;
	}
}

CRestoreRanks::~CRestoreRanks()
{
	if (m_ParseFailure)
	{
		dbg_msg("restore_ranks", "Some errors occured, not deleting: %s", m_FailureFileTmp);
	}
	else
	{
		if(fs_remove(m_FailureFileTmp))
			dbg_msg("restore_ranks", "failed to remove: %s", m_FailureFileTmp);
	}
}

void CRestoreRanks::parseJSON()
{
	if (m_ParseFailure)
		return;

	// exec the file
	IOHANDLE File = io_open(m_FailureFileTmp, IOFLAG_READ);

	if(File)
	{
		char *pLine;
		CLineReader lr;

		dbg_msg("restore_ranks", "reading '%s'", m_FailureFileTmp);
		lr.Init(File);

		while((pLine = lr.Get()))
		{
			dbg_msg("restore_ranks", "parsing: %s", pLine);

			try
			{
				json j = json::parse(pLine);

				char aType[32];
				auto pjTypeInfo = j.find("type");

				if (pjTypeInfo == j.end())
					throw std::domain_error("Can not handle json without type defined");


				str_copy(aType, pjTypeInfo->get<std::string>().c_str(), sizeof(aType));

				if (str_comp("rank", aType) == 0)
				{
					CSqlScoreData ScoreData;
					ScoreData.fromJSON(j);
					ExecSqlFunc<const CSqlScoreData&>(CRestoreRanks::SaveScore, false, ScoreData);
				}
				else if (str_comp("teamrank", aType) == 0)
				{
					CSqlTeamScoreData TeamScoreData;
					TeamScoreData.fromJSON(j);
					ExecSqlFunc<const CSqlTeamScoreData&>(CRestoreRanks::SaveTeamScore, false, TeamScoreData);
				}
				else
					throw std::domain_error("json neither of type rank nor teamrank");

			}
			catch (const std::domain_error& e)
			{
				dbg_msg("restore_ranks", "ERROR: Failed to parse json.");
				dbg_msg("restore_ranks", e.what());
				m_ParseFailure = true;
			}
			catch (std::invalid_argument& e)
			{
				dbg_msg("restore_ranks", "ERROR: Failed to parse json.");
				dbg_msg("restore_ranks", e.what());
				m_ParseFailure = true;
			}
		}
		io_close(File);
	}
	else
	{
		dbg_msg("restore_ranks", "failed to open '%s'", g_Config.m_SvSqlFailureFile);
		m_ParseFailure = true;
	}
}

bool CRestoreRanks::SaveScore(bool HandleFailure, CSqlServer* pSqlServer, const CSqlScoreData& Data)
{
	if (HandleFailure)
	{
		if (!g_Config.m_SvSqlFailureFile[0])
			return true;

		IOHANDLE File = io_open(g_Config.m_SvSqlFailureFile, IOFLAG_APPEND);
		if(File)
		{
			dbg_msg("sql", "ERROR: Could not save Score, writing insert to a file now...");

			std::string s = Data.toJSON().dump();
			io_write(File, s.c_str(), s.length());
			io_write_newline(File);
			io_close(File);

			return true;
		}
		dbg_msg("sql", "ERROR: Could not save Score, NOT even to a file");
		return false;
	}

	try
	{
		char aBuf[768];

		str_format(aBuf, sizeof(aBuf), "SELECT * FROM %s_race WHERE Map='%s' AND Name='%s' ORDER BY time ASC LIMIT 1;", pSqlServer->GetPrefix(), Data.m_Map.ClrStr(), Data.m_Name.ClrStr());
		pSqlServer->executeSqlQuery(aBuf);
		if(!pSqlServer->GetResults()->next())
		{
			str_format(aBuf, sizeof(aBuf), "SELECT Points FROM %s_maps WHERE Map ='%s'", pSqlServer->GetPrefix(), Data.m_Map.ClrStr());
			pSqlServer->executeSqlQuery(aBuf);

			if(pSqlServer->GetResults()->rowsCount() == 1)
			{
				pSqlServer->GetResults()->next();
				int points = (int)pSqlServer->GetResults()->getInt("Points");

				str_format(aBuf, sizeof(aBuf), "INSERT INTO %s_points(Name, Points) VALUES ('%s', '%d') ON duplicate key UPDATE Name=VALUES(Name), Points=Points+VALUES(Points);", pSqlServer->GetPrefix(), Data.m_Name.ClrStr(), points);
				pSqlServer->executeSql(aBuf);
			}
		}

		// if no entry found... create a new one
		str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_race(Map, Name, Timestamp, Time, Server, cp1, cp2, cp3, cp4, cp5, cp6, cp7, cp8, cp9, cp10, cp11, cp12, cp13, cp14, cp15, cp16, cp17, cp18, cp19, cp20, cp21, cp22, cp23, cp24, cp25) VALUES ('%s', '%s', '%s', '%.2f', '%s', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f');", pSqlServer->GetPrefix(), Data.m_Map.ClrStr(), Data.m_Name.ClrStr(), Data.m_aTimestamp, Data.m_Time, g_Config.m_SvSqlServerName, Data.m_aCpCurrent[0], Data.m_aCpCurrent[1], Data.m_aCpCurrent[2], Data.m_aCpCurrent[3], Data.m_aCpCurrent[4], Data.m_aCpCurrent[5], Data.m_aCpCurrent[6], Data.m_aCpCurrent[7], Data.m_aCpCurrent[8], Data.m_aCpCurrent[9], Data.m_aCpCurrent[10], Data.m_aCpCurrent[11], Data.m_aCpCurrent[12], Data.m_aCpCurrent[13], Data.m_aCpCurrent[14], Data.m_aCpCurrent[15], Data.m_aCpCurrent[16], Data.m_aCpCurrent[17], Data.m_aCpCurrent[18], Data.m_aCpCurrent[19], Data.m_aCpCurrent[20], Data.m_aCpCurrent[21], Data.m_aCpCurrent[22], Data.m_aCpCurrent[23], Data.m_aCpCurrent[24]);
		dbg_msg("sql", "%s", aBuf);
		pSqlServer->executeSql(aBuf);

		dbg_msg("sql", "Updating time done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not update time");
	}
	return false;
}


bool CRestoreRanks::SaveTeamScore(bool HandleFailure, CSqlServer* pSqlServer, const CSqlTeamScoreData& Data)
{
	if (HandleFailure)
	{
		if (!g_Config.m_SvSqlFailureFile[0])
			return true;

		dbg_msg("sql", "ERROR: Could not save TeamScore, writing insert to a file now...");

		IOHANDLE File = io_open(g_Config.m_SvSqlFailureFile, IOFLAG_APPEND);
		if(File)
		{
			std::string s = Data.toJSON().dump();
			io_write(File, s.c_str(), s.length());
			io_write_newline(File);
			io_close(File);
			return true;
		}
		return false;
	}

	try
	{
		char aBuf[2300];
		char aUpdateID[17];
		aUpdateID[0] = 0;

		str_format(aBuf, sizeof(aBuf), "SELECT Name, l.ID, Time FROM ((SELECT ID FROM %s_teamrace WHERE Map = '%s' AND Name = '%s') as l) LEFT JOIN %s_teamrace as r ON l.ID = r.ID ORDER BY ID;", pSqlServer->GetPrefix(), Data.m_Map.ClrStr(), Data.m_aNames[0].ClrStr(), pSqlServer->GetPrefix());
		pSqlServer->executeSqlQuery(aBuf);

		if (pSqlServer->GetResults()->rowsCount() > 0)
		{
			char aID[17];
			char aID2[17];
			char aName[64];
			unsigned int Count = 0;
			bool ValidNames = true;

			pSqlServer->GetResults()->first();
			float Time = (float)pSqlServer->GetResults()->getDouble("Time");
			strcpy(aID, pSqlServer->GetResults()->getString("ID").c_str());

			do
			{
				strcpy(aID2, pSqlServer->GetResults()->getString("ID").c_str());
				strcpy(aName, pSqlServer->GetResults()->getString("Name").c_str());
				sqlstr::ClearString(aName);
				if (str_comp(aID, aID2) != 0)
				{
					if (ValidNames && Count == Data.m_Size)
					{
						if (Data.m_Time < Time)
							strcpy(aUpdateID, aID);
						else
							goto end;
						break;
					}

					Time = (float)pSqlServer->GetResults()->getDouble("Time");
					ValidNames = true;
					Count = 0;
					strcpy(aID, aID2);
				}

				if (!ValidNames)
					continue;

				ValidNames = false;

				for(unsigned int i = 0; i < Data.m_Size; i++)
				{
					if (str_comp(aName, Data.m_aNames[i].ClrStr()) == 0)
					{
						ValidNames = true;
						Count++;
						break;
					}
				}
			} while (pSqlServer->GetResults()->next());

			if (ValidNames && Count == Data.m_Size)
			{
				if (Data.m_Time < Time)
					strcpy(aUpdateID, aID);
				else
					goto end;
			}
		}

		if (aUpdateID[0])
		{
			str_format(aBuf, sizeof(aBuf), "UPDATE %s_teamrace SET Time='%.2f' WHERE ID = '%s';", pSqlServer->GetPrefix(), Data.m_Time, aUpdateID);
			dbg_msg("sql", "%s", aBuf);
			pSqlServer->executeSql(aBuf);
		}
		else
		{
			pSqlServer->executeSql("SET @id = UUID();");

			for(unsigned int i = 0; i < Data.m_Size; i++)
			{
			// if no entry found... create a new one
				str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO %s_teamrace(Map, Name, Timestamp, Time, ID) VALUES ('%s', '%s', '%s', '%.2f', @id);", pSqlServer->GetPrefix(), Data.m_Map.ClrStr(), Data.m_aNames[i].ClrStr(), Data.m_aTimestamp, Data.m_Time);
				dbg_msg("sql", "%s", aBuf);
				pSqlServer->executeSql(aBuf);
			}
		}

		end:
		dbg_msg("sql", "Updating team time done");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
		dbg_msg("sql", "ERROR: Could not update time");
	}
	return false;
}


int main(int argc, const char** argv)
{
	#if !defined(CONF_PLATFORM_MACOSX) && !defined(FUZZING)
		dbg_enable_threaded();
	#endif

	dbg_logger_stdout();


	IKernel *pKernel = IKernel::Create();

	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER);
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_SERVER, argc, argv); // ignore_convention
	IConfig *pConfig = CreateConfig();

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfig);

		if(RegisterFail)
			return -1;
	}

	pConfig->Init();

	CSqlServer::RegisterCommands(pConsole);

	// execute autoexec file
	IOHANDLE file = pStorage->OpenFile("restore_ranks.cfg", IOFLAG_READ, IStorage::TYPE_ALL);
	if(file)
	{
		io_close(file);
		pConsole->ExecuteFile("restore_ranks.cfg");
	}
	else
	{
		dbg_msg("restore_ranks", "ERROR: failed to open restore_ranks.cfg");
		return 1;
	}

	// parse the command line arguments
	if(argc > 1) // ignore_convention
		pConsole->ParseArguments(argc-1, &argv[1]); // ignore_convention

	// process pending commands
	pConsole->StoreCommands(false);

	CRestoreRanks RR(pConsole, pConfig);
	RR.parseJSON();
	int Failure = RR.paresFailure();

	CSqlServer::deleteServers();

	delete pConsole;
	delete pConfig;

	return Failure;
}

#else

#include <iostream>

int main(int argc, const char** argv)
{
	std::cout << "You need to compile this tool with sql-support to make use of it." << std::endl;
}

#endif
