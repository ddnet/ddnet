#include <base/system.h>
#include <cstdlib>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <game/server/gamecontext.h>
#include <game/server/instagib/extra_columns.h>
#include <game/server/instagib/sql_stats_player.h>

#include "sql_stats.h"

CSqlStats::CSqlStats(CGameContext *pGameServer, CDbConnectionPool *pPool) :
	m_pPool(pPool),
	m_pGameServer(pGameServer),
	m_pServer(pGameServer->Server())
{
}

void CSqlStats::SetExtraColumns(CExtraColumns *pExtraColumns)
{
	m_pExtraColumns = pExtraColumns;
}

CSqlInstaData::~CSqlInstaData()
{
	dbg_msg("sql-thread", "round stats request destructor called");

	if(m_pExtraColumns)
	{
		dbg_msg("sql-thread", "free memory at %p", m_pExtraColumns);
		free(m_pExtraColumns);
		m_pExtraColumns = nullptr;
	}
}

void CSqlStats::SaveRoundStats(const char *pName, const char *pTable, CSqlStatsPlayer *pStats)
{
	auto Tmp = std::make_unique<CSqlSaveRoundStatsRequest>();

	Tmp->m_pExtraColumns = (CExtraColumns *)malloc(sizeof(CExtraColumns));
	mem_copy(Tmp->m_pExtraColumns, m_pExtraColumns, sizeof(CExtraColumns));
	dbg_msg("sql", "allocated memory at %p", Tmp->m_pExtraColumns);

	str_copy(Tmp->m_aName, pName);
	str_copy(Tmp->m_aTable, pTable);
	mem_copy(&Tmp->m_Stats, pStats, sizeof(Tmp->m_Stats));
	m_pPool->ExecuteWrite(SaveRoundStatsThread, std::move(Tmp), "save round stats");
}

bool CSqlStats::SaveRoundStatsThread(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	if(w != Write::NORMAL)
		return false;

	const CSqlSaveRoundStatsRequest *pData = dynamic_cast<const CSqlSaveRoundStatsRequest *>(pGameData);
	dbg_msg("sql-thread", "writing stats of player '%s'", pData->m_aName);
	dbg_msg("sql-thread", "extra columns %p", pData->m_pExtraColumns);

	char aBuf[4096];
	str_format(
		aBuf,
		sizeof(aBuf),
		"SELECT"
		" name, kills, deaths, spree,"
		" wins, losses, shots_fired, shots_hit %s "
		"FROM %s "
		"WHERE name = ?;",
		!pData->m_pExtraColumns ? "" : pData->m_pExtraColumns->SelectColumns(),
		pData->m_aTable);
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		dbg_msg("sql-thread", "prepare failed query: %s", aBuf);
		return true;
	}
	pSqlServer->BindString(1, pData->m_aName);
	pSqlServer->Print();

	dbg_msg("sql-thread", "select query: %s", aBuf);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		dbg_msg("sql-thread", "step failed");
		return true;
	}
	if(End)
	{
		dbg_msg("sql-thread", "inserting new record ...");
		// the _backup table is not used yet
		// because we guard the write mode at the top
		// to avoid complexity for now
		// but at some point we could write stats to a fallback database
		// and then merge them later
		str_format(
			aBuf,
			sizeof(aBuf),
			"%s INTO %s%s(" // INSERT INTO
			" name,"
			" kills, deaths, spree,"
			" wins, losses,"
			" shots_fired, shots_hit  %s"
			") VALUES ("
			" ?,"
			" ?, ?, ?,"
			" ?, ?,"
			" ?, ? %s"
			");",
			pSqlServer->InsertIgnore(),
			pData->m_aTable,
			w == Write::NORMAL ? "" : "_backup",
			!pData->m_pExtraColumns ? "" : pData->m_pExtraColumns->InsertColumns(),
			!pData->m_pExtraColumns ? "" : pData->m_pExtraColumns->InsertValues());

		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			dbg_msg("sql-thread", "prepare insert failed query=%s", aBuf);
			return true;
		}

		dbg_msg("sql-thread", "inserted query: %s", aBuf);

		int Offset = 1;
		pSqlServer->BindString(Offset++, pData->m_aName);
		pSqlServer->BindInt(Offset++, pData->m_Stats.m_Kills);
		pSqlServer->BindInt(Offset++, pData->m_Stats.m_Deaths);
		pSqlServer->BindInt(Offset++, pData->m_Stats.m_BestSpree);
		pSqlServer->BindInt(Offset++, pData->m_Stats.m_Wins);
		pSqlServer->BindInt(Offset++, pData->m_Stats.m_Losses);
		pSqlServer->BindInt(Offset++, pData->m_Stats.m_ShotsFired);
		pSqlServer->BindInt(Offset++, pData->m_Stats.m_ShotsHit);

		dbg_msg("sql-thread", "pre extra offset %d", Offset);

		if(pData->m_pExtraColumns)
			pData->m_pExtraColumns->InsertBindings(&Offset, pSqlServer, &pData->m_Stats);

		dbg_msg("sql-thread", "final offset %d", Offset);

		pSqlServer->Print();

		int NumInserted;
		if(pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		{
			return true;
		}
	}
	else
	{
		dbg_msg("sql-thread", "updating existing record ...");

		CSqlStatsPlayer MergeStats;
		// 1 is name that we don't really need for now
		int Offset = 2;
		MergeStats.m_Kills = pSqlServer->GetInt(Offset++);
		MergeStats.m_Deaths = pSqlServer->GetInt(Offset++);
		MergeStats.m_BestSpree = pSqlServer->GetInt(Offset++);
		MergeStats.m_Wins = pSqlServer->GetInt(Offset++);
		MergeStats.m_Losses = pSqlServer->GetInt(Offset++);
		MergeStats.m_ShotsFired = pSqlServer->GetInt(Offset++);
		MergeStats.m_ShotsHit = pSqlServer->GetInt(Offset++);

		dbg_msg("sql-thread", "loaded stats:");
		MergeStats.Dump("sql-thread");

		MergeStats.Merge(&pData->m_Stats);
		if(pData->m_pExtraColumns)
			pData->m_pExtraColumns->ReadAndMergeStats(&Offset, pSqlServer, &MergeStats, &pData->m_Stats);

		dbg_msg("sql-thread", "merged stats:");
		MergeStats.Dump("sql-thread");

		str_format(
			aBuf,
			sizeof(aBuf),
			"UPDATE %s%s "
			"SET"
			" kills = ?, deaths = ?, spree = ?,"
			" wins = ?, losses = ?,"
			" shots_fired = ?, shots_hit = ? %s"
			"WHERE name = ?;",
			pData->m_aTable,
			w == Write::NORMAL ? "" : "_backup",
			!pData->m_pExtraColumns ? "" : pData->m_pExtraColumns->UpdateColumns());

		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			dbg_msg("sql-thread", "prepare update failed query=%s", aBuf);
			return true;
		}

		Offset = 1;
		pSqlServer->BindInt(Offset++, MergeStats.m_Kills);
		pSqlServer->BindInt(Offset++, MergeStats.m_Deaths);
		pSqlServer->BindInt(Offset++, MergeStats.m_BestSpree);
		pSqlServer->BindInt(Offset++, MergeStats.m_Wins);
		pSqlServer->BindInt(Offset++, MergeStats.m_Losses);
		pSqlServer->BindInt(Offset++, MergeStats.m_ShotsFired);
		pSqlServer->BindInt(Offset++, MergeStats.m_ShotsHit);

		if(pData->m_pExtraColumns)
			pData->m_pExtraColumns->UpdateBindings(&Offset, pSqlServer, &MergeStats);

		pSqlServer->BindString(Offset++, pData->m_aName);

		pSqlServer->Print();

		int NumUpdated;
		if(pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
		{
			return true;
		}

		if(NumUpdated == 0 && pData->m_Stats.HasValues())
		{
			dbg_msg("sql-thread", "update failed no rows changed but got the following stats:");
			pData->m_Stats.Dump("sql-thread");
			return true;
		}
		else if(NumUpdated > 1)
		{
			dbg_msg("sql-thread", "affected %d rows when trying to update stats of one player!", NumUpdated);
			dbg_assert(false, "FATAL ERROR: your database is probably corrupted! Time to restore the backup.");
			return true;
		}
	}
	return false;
}

void CSqlStats::CreateTable(const char *pName)
{
	auto Tmp = std::make_unique<CSqlCreateTableRequest>();
	str_copy(Tmp->m_aName, pName);
	Tmp->m_aColumns[0] = '\0';
	if(m_pExtraColumns)
		str_copy(Tmp->m_aColumns, m_pExtraColumns->CreateTable());
	m_pPool->ExecuteWrite(CreateTableThread, std::move(Tmp), "create table");
}

bool CSqlStats::CreateTableThread(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	if(w == Write::NORMAL_FAILED)
	{
		dbg_assert(false, "CreateTableThread failed to write");
		return true;
	}
	const CSqlCreateTableRequest *pData = dynamic_cast<const CSqlCreateTableRequest *>(pGameData);

	// autoincrement not recommended by sqlite3
	// also its hard to be portable accross mysql and sqlite3
	// ddnet also uses any kind of unicode playername in the points update query

	char aBuf[4096];
	str_format(aBuf, sizeof(aBuf),
		"CREATE TABLE IF NOT EXISTS %s%s("
		"name        VARCHAR(%d)   COLLATE %s  NOT NULL,"
		"kills       INTEGER       DEFAULT 0,"
		"deaths      INTEGER       DEFAULT 0,"
		"spree       INTEGER       DEFAULT 0,"
		"wins        INTEGER       DEFAULT 0,"
		"losses      INTEGER       DEFAULT 0,"
		"shots_fired INTEGER       DEFAULT 0,"
		"shots_hit   INTEGER       DEFAULT 0,"
		"%s"
		"PRIMARY KEY (name)"
		");",
		pData->m_aName,
		w == Write::NORMAL ? "" : "_backup",
		MAX_NAME_LENGTH_SQL,
		pSqlServer->BinaryCollate(),
		pData->m_aColumns);

	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->Print();
	int NumInserted;
	return pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize);
}
