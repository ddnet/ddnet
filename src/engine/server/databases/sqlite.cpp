#include "connection.h"

#include <sqlite3.h>

#include <base/math.h>
#include <engine/console.h>

#include <atomic>

class CSqliteConnection : public IDbConnection
{
public:
	CSqliteConnection(const char *pFilename, bool Setup);
	virtual ~CSqliteConnection();
	virtual void Print(IConsole *pConsole, const char *Mode);

	virtual CSqliteConnection *Copy();

	virtual const char *BinaryCollate() const { return "BINARY"; }
	virtual void ToUnixTimestamp(const char *pTimestamp, char *aBuf, unsigned int BufferSize);
	virtual const char *InsertTimestampAsUtc() const { return "DATETIME(?, 'utc')"; }
	virtual const char *CollateNocase() const { return "? COLLATE NOCASE"; }
	virtual const char *InsertIgnore() const { return "INSERT OR IGNORE"; };
	virtual const char *Random() const { return "RANDOM()"; };
	virtual const char *MedianMapTime(char *pBuffer, int BufferSize) const;

	virtual bool Connect(char *pError, int ErrorSize);
	virtual void Disconnect();

	virtual bool PrepareStatement(const char *pStmt, char *pError, int ErrorSize);

	virtual void BindString(int Idx, const char *pString);
	virtual void BindBlob(int Idx, unsigned char *pBlob, int Size);
	virtual void BindInt(int Idx, int Value);
	virtual void BindFloat(int Idx, float Value);

	virtual void Print();
	virtual bool Step(bool *pEnd, char *pError, int ErrorSize);
	virtual bool ExecuteUpdate(int *pNumUpdated, char *pError, int ErrorSize);

	virtual bool IsNull(int Col);
	virtual float GetFloat(int Col);
	virtual int GetInt(int Col);
	virtual void GetString(int Col, char *pBuffer, int BufferSize);
	// passing a negative buffer size is undefined behavior
	virtual int GetBlob(int Col, unsigned char *pBuffer, int BufferSize);

	virtual bool AddPoints(const char *pPlayer, int Points, char *pError, int ErrorSize);

private:
	// copy of config vars
	char m_aFilename[512];
	bool m_Setup;

	sqlite3 *m_pDb;
	sqlite3_stmt *m_pStmt;
	bool m_Done; // no more rows available for Step
	// returns false, if the query succeeded
	bool Execute(const char *pQuery, char *pError, int ErrorSize);

	// returns true if an error was formatted
	bool FormatError(int Result, char *pError, int ErrorSize);
	void AssertNoError(int Result);

	std::atomic_bool m_InUse;
};

CSqliteConnection::CSqliteConnection(const char *pFilename, bool Setup) :
	IDbConnection("record"),
	m_Setup(Setup),
	m_pDb(nullptr),
	m_pStmt(nullptr),
	m_Done(true),
	m_InUse(false)
{
	str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
}

CSqliteConnection::~CSqliteConnection()
{
	if(m_pStmt != nullptr)
		sqlite3_finalize(m_pStmt);
	sqlite3_close(m_pDb);
	m_pDb = nullptr;
}

void CSqliteConnection::Print(IConsole *pConsole, const char *Mode)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SQLite-%s: DB: '%s'",
		Mode, m_aFilename);
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CSqliteConnection::ToUnixTimestamp(const char *pTimestamp, char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize, "strftime('%%s', %s)", pTimestamp);
}

CSqliteConnection *CSqliteConnection::Copy()
{
	return new CSqliteConnection(m_aFilename, m_Setup);
}

bool CSqliteConnection::Connect(char *pError, int ErrorSize)
{
	if(m_InUse.exchange(true))
	{
		dbg_assert(0, "Tried connecting while the connection is in use");
	}

	if(m_pDb != nullptr)
	{
		return false;
	}

	int Result = sqlite3_open(m_aFilename, &m_pDb);
	if(Result != SQLITE_OK)
	{
		str_format(pError, ErrorSize, "Can't open sqlite database: '%s'", sqlite3_errmsg(m_pDb));
		return true;
	}

	// wait for database to unlock so we don't have to handle SQLITE_BUSY errors
	sqlite3_busy_timeout(m_pDb, -1);

	if(m_Setup)
	{
		char aBuf[1024];
		FormatCreateRace(aBuf, sizeof(aBuf));
		if(Execute(aBuf, pError, ErrorSize))
			return true;
		FormatCreateTeamrace(aBuf, sizeof(aBuf), "BLOB");
		if(Execute(aBuf, pError, ErrorSize))
			return true;
		FormatCreateMaps(aBuf, sizeof(aBuf));
		if(Execute(aBuf, pError, ErrorSize))
			return true;
		FormatCreateSaves(aBuf, sizeof(aBuf));
		if(Execute(aBuf, pError, ErrorSize))
			return true;
		FormatCreatePoints(aBuf, sizeof(aBuf));
		if(Execute(aBuf, pError, ErrorSize))
			return true;
		m_Setup = false;
	}
	return false;
}

void CSqliteConnection::Disconnect()
{
	if(m_pStmt != nullptr)
		sqlite3_finalize(m_pStmt);
	m_pStmt = nullptr;
	m_InUse.store(false);
}

bool CSqliteConnection::PrepareStatement(const char *pStmt, char *pError, int ErrorSize)
{
	if(m_pStmt != nullptr)
		sqlite3_finalize(m_pStmt);
	m_pStmt = nullptr;
	int Result = sqlite3_prepare_v2(
		m_pDb,
		pStmt,
		-1, // pStmt can be any length
		&m_pStmt,
		NULL);
	if(FormatError(Result, pError, ErrorSize))
	{
		return true;
	}
	m_Done = false;
	return false;
}

void CSqliteConnection::BindString(int Idx, const char *pString)
{
	int Result = sqlite3_bind_text(m_pStmt, Idx, pString, -1, NULL);
	AssertNoError(Result);
	m_Done = false;
}

void CSqliteConnection::BindBlob(int Idx, unsigned char *pBlob, int Size)
{
	int Result = sqlite3_bind_blob(m_pStmt, Idx, pBlob, Size, NULL);
	AssertNoError(Result);
	m_Done = false;
}

void CSqliteConnection::BindInt(int Idx, int Value)
{
	int Result = sqlite3_bind_int(m_pStmt, Idx, Value);
	AssertNoError(Result);
	m_Done = false;
}

void CSqliteConnection::BindFloat(int Idx, float Value)
{
	int Result = sqlite3_bind_double(m_pStmt, Idx, (double)Value);
	AssertNoError(Result);
	m_Done = false;
}

// TODO(2020-09-07): remove extern declaration, when all supported systems ship SQLite3 version 3.14 or above
#if defined(__GNUC__)
extern char *sqlite3_expanded_sql(sqlite3_stmt *pStmt) __attribute__((weak));
#endif

void CSqliteConnection::Print()
{
	if(m_pStmt != nullptr && sqlite3_expanded_sql != nullptr)
	{
		char *pExpandedStmt = sqlite3_expanded_sql(m_pStmt);
		dbg_msg("sql", "SQLite statement: %s", pExpandedStmt);
		sqlite3_free(pExpandedStmt);
	}
}

bool CSqliteConnection::Step(bool *pEnd, char *pError, int ErrorSize)
{
	if(m_Done)
	{
		*pEnd = true;
		return false;
	}
	int Result = sqlite3_step(m_pStmt);
	if(Result == SQLITE_ROW)
	{
		*pEnd = false;
		return false;
	}
	else if(Result == SQLITE_DONE)
	{
		m_Done = true;
		*pEnd = true;
		return false;
	}
	else
	{
		if(FormatError(Result, pError, ErrorSize))
		{
			return true;
		}
	}
	*pEnd = true;
	return false;
}

bool CSqliteConnection::ExecuteUpdate(int *pNumUpdated, char *pError, int ErrorSize)
{
	bool End;
	if(Step(&End, pError, ErrorSize))
	{
		return true;
	}
	*pNumUpdated = sqlite3_changes(m_pDb);
	return false;
}

bool CSqliteConnection::IsNull(int Col)
{
	return sqlite3_column_type(m_pStmt, Col - 1) == SQLITE_NULL;
}

float CSqliteConnection::GetFloat(int Col)
{
	return (float)sqlite3_column_double(m_pStmt, Col - 1);
}

int CSqliteConnection::GetInt(int Col)
{
	return sqlite3_column_int(m_pStmt, Col - 1);
}

void CSqliteConnection::GetString(int Col, char *pBuffer, int BufferSize)
{
	str_copy(pBuffer, (const char *)sqlite3_column_text(m_pStmt, Col - 1), BufferSize);
}

int CSqliteConnection::GetBlob(int Col, unsigned char *pBuffer, int BufferSize)
{
	int Size = sqlite3_column_bytes(m_pStmt, Col - 1);
	Size = minimum(Size, BufferSize);
	mem_copy(pBuffer, sqlite3_column_blob(m_pStmt, Col - 1), Size);
	return Size;
}

const char *CSqliteConnection::MedianMapTime(char *pBuffer, int BufferSize) const
{
	str_format(pBuffer, BufferSize,
		"SELECT AVG("
		"  CASE counter %% 2 "
		"    WHEN 0 THEN CASE WHEN rn IN (counter / 2, counter / 2 + 1) THEN Time END "
		"    WHEN 1 THEN CASE WHEN rn = counter / 2 + 1 THEN Time END END) "
		"  OVER (PARTITION BY Map) AS Median "
		"FROM ("
		"  SELECT *, ROW_NUMBER() "
		"  OVER (PARTITION BY Map ORDER BY Time) rn, COUNT(*) "
		"  OVER (PARTITION BY Map) counter "
		"  FROM %s_race where Map = l.Map) as r",
		GetPrefix());
	return pBuffer;
}

bool CSqliteConnection::Execute(const char *pQuery, char *pError, int ErrorSize)
{
	char *pErrorMsg;
	int Result = sqlite3_exec(m_pDb, pQuery, NULL, NULL, &pErrorMsg);
	if(Result != SQLITE_OK)
	{
		str_format(pError, ErrorSize, "error executing query: '%s'", pErrorMsg);
		sqlite3_free(pErrorMsg);
		return true;
	}
	return false;
}

bool CSqliteConnection::FormatError(int Result, char *pError, int ErrorSize)
{
	if(Result != SQLITE_OK)
	{
		str_copy(pError, sqlite3_errmsg(m_pDb), ErrorSize);
		return true;
	}
	return false;
}

void CSqliteConnection::AssertNoError(int Result)
{
	char aBuf[128];
	if(FormatError(Result, aBuf, sizeof(aBuf)))
	{
		dbg_msg("sqlite", "unexpected sqlite error: %s", aBuf);
		dbg_assert(0, "sqlite error");
	}
}

bool CSqliteConnection::AddPoints(const char *pPlayer, int Points, char *pError, int ErrorSize)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"INSERT INTO %s_points(Name, Points) "
		"VALUES (?, ?) "
		"ON CONFLICT(Name) DO UPDATE SET Points=Points+?;",
		GetPrefix());
	if(PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	BindString(1, pPlayer);
	BindInt(2, Points);
	BindInt(3, Points);
	bool End;
	if(Step(&End, pError, ErrorSize))
	{
		return true;
	}
	return false;
}

IDbConnection *CreateSqliteConnection(const char *pFilename, bool Setup)
{
	return new CSqliteConnection(pFilename, Setup);
}
