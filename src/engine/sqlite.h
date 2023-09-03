#ifndef ENGINE_SQLITE_H
#define ENGINE_SQLITE_H
#include <memory>

struct sqlite3;
struct sqlite3_stmt;
class IConsole;
class IStorage;

class CSqliteDeleter
{
public:
	void operator()(sqlite3 *pSqlite);
};
class CSqliteStmtDeleter
{
public:
	void operator()(sqlite3_stmt *pStmt);
};
typedef std::unique_ptr<sqlite3, CSqliteDeleter> CSqlite;
typedef std::unique_ptr<sqlite3_stmt, CSqliteStmtDeleter> CSqliteStmt;

int SqliteHandleError(IConsole *pConsole, int Error, sqlite3 *pSqlite, const char *pContext);
#define SQLITE_HANDLE_ERROR(x) SqliteHandleError(pConsole, x, &*pSqlite, #x)

CSqlite SqliteOpen(IConsole *pConsole, IStorage *pStorage, const char *pPath);
CSqliteStmt SqlitePrepare(IConsole *pConsole, sqlite3 *pSqlite, const char *pStatement);
#endif // ENGINE_SQLITE_H
