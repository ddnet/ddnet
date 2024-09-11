#ifndef GAME_SERVER_INSTAGIB_EXTRA_COLUMNS_H
#define GAME_SERVER_INSTAGIB_EXTRA_COLUMNS_H

#include <engine/server/databases/connection.h>
#include <game/server/instagib/sql_stats_player.h>

class CExtraColumns
{
public:
	virtual ~CExtraColumns() = default;

	/*
		CreateTable

		Should return a SQL string that will be used within CREATE TABLE

		Example: "kills INTEGER DEFAULT 0, deaths INTEGER DEFAULT 0"
	*/
	virtual const char *CreateTable() = 0;

	/*
		SelectColumns

		Should return a SQL string that will be used within a SELECT statament

		Example: "name, kills, deaths"
	*/
	virtual const char *SelectColumns() = 0;

	/*
		InsertColumns

		Should return a SQL string that will be used within a INSERT statement
		only the first part that lists the columns that will be inserted

		Example: "name, kills, deaths"
	*/
	virtual const char *InsertColumns() = 0;

	/*
		UpdateColumns

		Should return a SQL string that will be used within a UPDATE statement
		only the first part that lists the column assignments and their placeholders
		that will be set

		Example: "name = ?, kills = ?, deaths = ?"
	*/
	virtual const char *UpdateColumns() = 0;

	/*
		InsertValues

		Should return a SQL string that will be used within a INSERT statement
		only the second part that lists the values or placeholders that will be inserted

		Example: "?, ?, 0"
	*/
	virtual const char *InsertValues() = 0;

	/*
		InsertBindings

		Arguments:
			pOffset - Starting offset to bind the first parameter to. Its value to be increment for every value you bind.
			pSqlServer - IDbConnection that should be used for binding

		Callback that binds the placeholders set in InsertValues()

		Example binding code:

		pSqlServer->BindInt((*pOffset)++, pStats->m_Kills);
		pSqlServer->BindInt((*pOffset)++, pStats->m_Deaths);
	*/
	virtual void InsertBindings(int *pOffset, IDbConnection *pSqlServer, const CSqlStatsPlayer *pStats) = 0;

	/*
		UpdateBindings

		Arguments:
			pOffset - Starting offset to bind the first parameter to. Its value to be increment for every value you bind.
			pSqlServer - IDbConnection that should be used for binding

		Callback that binds the placeholders set in UpdateColumns()

		Example binding code:

		pSqlServer->BindInt((*pOffset)++, pStats->m_Kills);
		pSqlServer->BindInt((*pOffset)++, pStats->m_Deaths);
	*/
	virtual void UpdateBindings(int *pOffset, IDbConnection *pSqlServer, const CSqlStatsPlayer *pStats) = 0;

	/*
		ReadAndMergeStats

		Arguments:
			pOffset - Starting offset to bind the first parameter to. Its value to be increment for every value you bind.
			pSqlServer - IDbConnection that should be used for reading the values from
			pOutputStats - stats object that should be written to
			pNewStats - stats object with the stats from the current round should be red from

		Callback that reads the values from the previous SELECT statement
		and merges them into an existing stats object

		Example merge code:

		pOutputStats->m_Kills = pSqlServer->GetInt((*pOffset)++) + pNewStats->m_Kills;
		pOutputStats->m_Deaths = pSqlServer->GetInt((*pOffset)++) + pNewStats->m_Deaths;

		Or use one of the merge helpers:

		pOutputStats->m_Deaths = MergeIntAdd(pSqlServer->GetInt((*pOffset)++), pNewStats->m_Deaths);
	*/
	virtual void ReadAndMergeStats(int *pOffset, IDbConnection *pSqlServer, CSqlStatsPlayer *pOutputStats, const CSqlStatsPlayer *pNewStats) = 0;

	int MergeIntAdd(int Current, int Other)
	{
		return Current + Other;
	}

	int MergeIntHighest(int Current, int Other)
	{
		if(Current > Other)
			return Current;
		return Other;
	}

	int MergeIntLowest(int Current, int Other)
	{
		if(Current < Other)
			return Current;
		return Other;
	}

	float MergeFloatHighest(float Current, float Other)
	{
		if(Current > Other)
			return Current;
		return Other;
	}

	float MergeFloatLowest(float Current, float Other)
	{
		if(Current < Other)
			return Current;
		return Other;
	}
};

#endif
