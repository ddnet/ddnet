#ifndef GAME_SERVER_SQLDATA_H
#define GAME_SERVER_SQLDATA_H

#include <string>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/external/json/json.h>
#include <engine/shared/sql_string_helpers.h>

#include "../score.h"

using json = nlohmann::json;


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
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
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

	void fromJSON(const json& j)
	// throws std::domain_error and std::invalid_argument
	{
		if (str_comp("rank", j["type"].get<std::string>().c_str()))
			throw std::domain_error("JSON-Object not of type rank.");

		m_Map = j["Map"].get<std::string>().c_str();
		m_Name = j["Name"].get<std::string>().c_str();
		std::string timestamp = j["Timestamp"];
		str_copy(m_aTimestamp, timestamp.c_str(), sizeof(m_aTimestamp));
		m_Time = j["Time"];
		m_ServerName = j["Server"].get<std::string>().c_str();

		for (int i = 0; i < 25; i++)
		{
			char aBuf[5];
			str_format(aBuf, sizeof(aBuf), "cp%d", i);
			m_aCpCurrent[i] = j[aBuf].get<float>();
		}
	}

	void fromJSON(const char* pJStr)
	// throws std::domain_error and std::invalid_argument
	{
		fromJSON(json::parse(pJStr));
	}

};

struct CSqlTeamScoreData : CSqlData
{
	unsigned int m_Size;
	int m_aClientIDs[MAX_CLIENTS];
	sqlstr::CSqlString<MAX_NAME_LENGTH> m_aNames [MAX_CLIENTS];
	float m_Time;
	char m_aTimestamp [20];

	CSqlTeamScoreData()
	{
		sqlstr::getTimeStamp(m_aTimestamp, sizeof(m_aTimestamp));
	}

	json toJSON() const
	{
		json j = {
			{"type", "teamrank"},
			{"Map", m_Map.Str()},
			{"Timestamp", m_aTimestamp},
			{"Time", m_Time}
		};

		json json_names = json::array();

		for (unsigned int i = 0; i < m_Size; i++)
		{
			json_names += m_aNames[i].Str();
		}
		j["Names"] = json_names;
		return j;
	}

	void fromJSON(const json& j)
	// throws std::domain_error and std::invalid_argument
	{
		if (str_comp("teamrank", j["type"].get<std::string>().c_str()))
			throw std::domain_error("JSON-Object not of type teamrank.");

		m_Map = j["Map"].get<std::string>().c_str();
		std::string timestamp = j["Timestamp"];
		str_copy(m_aTimestamp, timestamp.c_str(), sizeof(m_aTimestamp));
		m_Time = j["Time"];

		m_Size = 0;
		for (const json& name : j["Names"])
		{
			if (m_Size == MAX_CLIENTS)
				break;
			m_aNames[m_Size++] = name.get<std::string>().c_str();
		}
	}

	void fromJSON(const char* pJStr)
	// throws std::domain_error and std::invalid_argument
	{
		fromJSON(json::parse(pJStr));
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
