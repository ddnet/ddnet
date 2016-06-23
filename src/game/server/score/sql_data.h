#ifndef GAME_SERVER_SQLDATA_H
#define GAME_SERVER_SQLDATA_H

#include <string>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/external/json/json.h>
#include <engine/server/sql_string_helpers.h>

#include "../score.h"

using json = nlohmann::json;


// generic implementation to provide gameserver and server
struct CSqlData
{
	CSqlData() : m_Map(g_Config.m_SvMap) {}

	virtual ~CSqlData() {}

	sqlstr::CSqlString<128> m_Map;

};

struct CSqlPlayerData : CSqlData
{
	int m_ClientID;
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_Name;
};

// used for mapvote and mapinfo
struct CSqlMapData : CSqlData
{
	int m_ClientID;

	sqlstr::CSqlString<128> m_RequestedMap;
	char m_aFuzzyMap[128];
};

struct CSqlScoreData : CSqlData
{
	int m_ClientID;

	sqlstr::CSqlString<MAX_NAME_LENGTH> m_Name;

	float m_Time;
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer [MAX_NAME_LENGTH];
	char m_aTimestamp [20];
	sqlstr::CSqlString<sizeof(g_Config.m_SvSqlServerName)> m_ServerName;

	CSqlScoreData()
	{
		sqlstr::getTimeStamp(m_aTimestamp, sizeof(m_aTimestamp));
		m_ServerName = g_Config.m_SvSqlServerName;
	}

	json toJSON() const
	{
		json j = {
			{"type", "rank"},
			{"Map", m_Map.Str()},
			{"Name", m_Name.Str()},
			{"Timestamp", m_aTimestamp},
			{"Time", m_Time},
			{"Server", m_ServerName.Str()}
		};
		for (int i = 0; i < 25; i++)
		{
			char aBuf[5];
			str_format(aBuf, sizeof(aBuf), "cp%d", i);
			j[aBuf] = m_aCpCurrent[i];
		}
		return j;
	}

	void fromJSON(const char* pJStr)
	{
		try
		{
			json j = json::parse(pJStr);
			m_Map = j["Map"].get<std::string>().c_str();
			m_Name = j["Name"].get<std::string>().c_str();
			std::string timestamp = j["Timestamp"];
			str_copy(m_aTimestamp, timestamp.c_str(), timestamp.length());
			m_Time = j["Time"];
			m_ServerName = j["Server"].get<std::string>().c_str();
		}
		catch (const std::domain_error& e)
		{
			dbg_msg("sql", "ERROR: Failed to parse rank json.");
			dbg_msg("sql", e.what());
		}
		catch (std::invalid_argument& e)
		{
			dbg_msg("sql", "ERROR: Failed to parse rank json.");
			dbg_msg("sql", e.what());
		}
	}

};

struct CSqlTeamScoreData : CSqlData
{
	unsigned int m_Size;
	int m_aClientIDs[MAX_CLIENTS];
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_aNames [MAX_CLIENTS];
	float m_Time;
	char m_aTimestamp [20];
	sqlstr::CSqlString<sizeof(g_Config.m_SvSqlServerName)> m_ServerName;

	CSqlTeamScoreData()
	{
		sqlstr::getTimeStamp(m_aTimestamp, sizeof(m_aTimestamp));
		m_ServerName = g_Config.m_SvSqlServerName;
	}

	json toJSON() const
	{
		json j = {
			{"type", "teamrank"},
			{"Map", m_Map.Str()},
			{"Server", m_ServerName.Str()},
			{"Timestamp", m_aTimestamp},
			{"Time", m_Time}
		};

		json json_names = json::array();

		for (int i = 0; i < m_Size; i++)
		{
			json_names += m_aNames[i].Str();
		}
		j["Names"] = json_names;
		return j;
	}

	void fromJSON(const char* pJStr)
	{
		try
		{
			json j = json::parse(pJStr);
			m_Map = j["Map"].get<std::string>().c_str();
			std::string timestamp = j["Timestamp"];
			str_copy(m_aTimestamp, timestamp.c_str(), timestamp.length());
			m_Time = j["Time"];
			m_ServerName = j["Server"].get<std::string>().c_str();

			int i = 0;
			for (json& name : j["Names"])
			{
				if (i == MAX_CLIENTS)
					break;
				m_aNames[i++] = name.get<std::string>().c_str();
			}
		}
		catch (const std::domain_error& e)
		{
			dbg_msg("sql", "ERROR: Failed to parse teamrank json.");
			dbg_msg("sql", e.what());
		}
		catch (std::invalid_argument& e)
		{
			dbg_msg("sql", "ERROR: Failed to parse teamrank json.");
			dbg_msg("sql", e.what());
		}
	}
};

struct CSqlTeamSave : CSqlData
{
	int m_Team;
	int m_ClientID;
	sqlstr::CSqlString<128> m_Code;
	char m_Server[5];
};

struct CSqlTeamLoad : CSqlData
{
	sqlstr::CSqlString<128> m_Code;
	int m_ClientID;
};

#endif
