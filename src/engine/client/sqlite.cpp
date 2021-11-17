#include <base/system.h>
#include <engine/console.h>
#include <engine/sqlite.h>

#include <sqlite3.h>

void CSqliteDeleter::operator()(sqlite3 *pSqlite)
{
	sqlite3_close(pSqlite);
}

void CSqliteStmtDeleter::operator()(sqlite3_stmt *pStmt)
{
	sqlite3_finalize(pStmt);
}

int SqliteHandleError(IConsole *pConsole, int Error, sqlite3 *pSqlite, const char *pContext)
{
	if(Error != SQLITE_OK && Error != SQLITE_DONE && Error != SQLITE_ROW)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%s at %s", sqlite3_errmsg(pSqlite), pContext);
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sqlite3", aBuf);
	}
	return Error;
}

CSqlite SqliteOpen(IConsole *pConsole, IStorage *pStorage, const char *pPath)
{
	char aFullPath[IO_MAX_PATH_LENGTH];
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, pPath, aFullPath, sizeof(aFullPath));
	sqlite3 *pSqlite = nullptr;
	const bool ErrorOpening = SQLITE_HANDLE_ERROR(sqlite3_open(aFullPath, &pSqlite)) != SQLITE_OK;
	// Even on error, the database is initialized and needs to be freed.
	// Except on allocation failure, but then it'll be nullptr which is
	// also fine.
	CSqlite pResult{pSqlite};
	if(ErrorOpening)
	{
		return nullptr;
	}
	bool Error = false;
	Error = Error || SQLITE_HANDLE_ERROR(sqlite3_exec(pSqlite, "PRAGMA journal_mode = WAL", nullptr, nullptr, nullptr));
	Error = Error || SQLITE_HANDLE_ERROR(sqlite3_exec(pSqlite, "PRAGMA synchronous = NORMAL", nullptr, nullptr, nullptr));
	if(Error)
	{
		return nullptr;
	}
	return pResult;
}

CSqliteStmt SqlitePrepare(IConsole *pConsole, sqlite3 *pSqlite, const char *pStatement)
{
	sqlite3_stmt *pTemp;
	if(SQLITE_HANDLE_ERROR(sqlite3_prepare_v2(pSqlite, pStatement, -1, &pTemp, nullptr)))
	{
		return nullptr;
	}
	return CSqliteStmt{pTemp};
}
