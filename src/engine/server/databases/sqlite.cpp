#include "sqlite.h"

#include <base/math.h>

#include <stdexcept>
#include <sqlite3.h>

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

CSqliteConnection *CSqliteConnection::Copy()
{
	return new CSqliteConnection(m_aFilename, m_Setup);
}

IDbConnection::Status CSqliteConnection::Connect()
{
	if(m_InUse.exchange(true))
		return Status::IN_USE;

	if(m_pDb != nullptr)
		return Status::SUCCESS;

	int Result = sqlite3_open(m_aFilename, &m_pDb);
	if(Result != SQLITE_OK)
	{
		dbg_msg("sql", "Can't open sqlite database: '%s'", sqlite3_errmsg(m_pDb));
		return Status::ERROR;
	}

	// wait for database to unlock so we don't have to handle SQLITE_BUSY errors
	sqlite3_busy_timeout(m_pDb, -1);

	if(m_Setup)
	{
		char aBuf[1024];
		FormatCreateRace(aBuf, sizeof(aBuf));
		if(!Execute(aBuf))
			return Status::ERROR;
		FormatCreateTeamrace(aBuf, sizeof(aBuf), "BLOB");
		if(!Execute(aBuf))
			return Status::ERROR;
		FormatCreateMaps(aBuf, sizeof(aBuf));
		if(!Execute(aBuf))
			return Status::ERROR;
		FormatCreateSaves(aBuf, sizeof(aBuf));
		if(!Execute(aBuf))
			return Status::ERROR;
		FormatCreatePoints(aBuf, sizeof(aBuf));
		if(!Execute(aBuf))
			return Status::ERROR;
		m_Setup = false;
	}
	return Status::SUCCESS;
}

void CSqliteConnection::Disconnect()
{
	if(m_pStmt != nullptr)
		sqlite3_finalize(m_pStmt);
	m_pStmt = nullptr;
	m_InUse.store(false);
}

void CSqliteConnection::Lock(const char *pTable)
{
}

void CSqliteConnection::Unlock()
{
}

void CSqliteConnection::PrepareStatement(const char *pStmt)
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
	ExceptionOnError(Result);
	m_Done = false;
}

void CSqliteConnection::BindString(int Idx, const char *pString)
{
	int Result = sqlite3_bind_text(m_pStmt, Idx, pString, -1, NULL);
	ExceptionOnError(Result);
	m_Done = false;
}

void CSqliteConnection::BindInt(int Idx, int Value)
{
	int Result = sqlite3_bind_int(m_pStmt, Idx, Value);
	ExceptionOnError(Result);
	m_Done = false;
}

bool CSqliteConnection::Step()
{
	if(m_Done)
		return false;
	int Result = sqlite3_step(m_pStmt);
	if(Result == SQLITE_ROW)
	{
		return true;
	}
	else if(Result == SQLITE_DONE)
	{
		m_Done = true;
		return false;
	}
	else
	{
		ExceptionOnError(Result);
	}
	return false;
}

bool CSqliteConnection::IsNull(int Col) const
{
	return sqlite3_column_type(m_pStmt, Col - 1) == SQLITE_NULL;
}

float CSqliteConnection::GetFloat(int Col) const
{
	return (float)sqlite3_column_double(m_pStmt, Col - 1);
}

int CSqliteConnection::GetInt(int Col) const
{
	return sqlite3_column_int(m_pStmt, Col - 1);
}

void CSqliteConnection::GetString(int Col, char *pBuffer, int BufferSize) const
{
	str_copy(pBuffer, (const char *)sqlite3_column_text(m_pStmt, Col - 1), BufferSize);
}

int CSqliteConnection::GetBlob(int Col, unsigned char *pBuffer, int BufferSize) const
{
	int Size = sqlite3_column_bytes(m_pStmt, Col - 1);
	Size = minimum(Size, BufferSize);
	mem_copy(pBuffer, sqlite3_column_blob(m_pStmt, Col - 1), Size);
	return Size;
}

bool CSqliteConnection::Execute(const char *pQuery)
{
	char *pErrorMsg;
	int Result = sqlite3_exec(m_pDb, pQuery, NULL, NULL, &pErrorMsg);
	if(Result != SQLITE_OK)
	{
		dbg_msg("sql", "error executing query: '%s'", pErrorMsg);
		sqlite3_free(pErrorMsg);
	}
	return Result == SQLITE_OK;
}

void CSqliteConnection::ExceptionOnError(int Result)
{
	if(Result != SQLITE_OK)
	{
		throw std::runtime_error(sqlite3_errmsg(m_pDb));
	}
}
