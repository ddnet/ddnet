#include "connection.h"

#include <engine/server/databases/connection_pool.h>

#if defined(CONF_POSTGRESQL)
#include <base/dbg.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>

#include <libpq-fe.h>

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <memory>
#include <string>
#include <vector>

enum
{
	// OID of the `bytea` type from the PostgreSQL catalog, stable across versions.
	PG_BYTEA_OID = 17,
};

class CPostgresqlConnection : public IDbConnection
{
public:
	explicit CPostgresqlConnection(CPostgresqlConfig Config);
	~CPostgresqlConnection() override;
	void Print(const char *pMode) override;

	const char *BinaryCollate() const override { return "\"C\""; }
	void ToUnixTimestamp(const char *pTimestamp, char *aBuf, unsigned int BufferSize) override;
	std::string Placeholder(int Idx) const override { return "$" + std::to_string(Idx); }
	std::string InsertTimestampAsUtc(int Idx) const override { return Placeholder(Idx); }
	std::string LikeNocase(int Idx) const override { return "ILIKE " + Placeholder(Idx); }
	const char *InsertIgnore() const override { return "INSERT"; }
	const char *InsertIgnoreEnd() const override { return " ON CONFLICT DO NOTHING"; }
	const char *Random() const override { return "RANDOM()"; }
	const char *MedianMapTime(char *pBuffer, int BufferSize) const override;
	const char *False() const override { return "FALSE"; }
	const char *True() const override { return "TRUE"; }

	bool Connect(char *pError, int ErrorSize) override;
	void Disconnect() override;

	bool PrepareStatement(const char *pStmt, char *pError, int ErrorSize) override;

	void BindString(int Idx, const char *pString) override;
	void BindBlob(int Idx, unsigned char *pBlob, int Size) override;
	void BindInt(int Idx, int Value) override;
	void BindInt64(int Idx, int64_t Value) override;
	void BindFloat(int Idx, float Value) override;
	void BindNull(int Idx) override;

	void Print() override {}
	bool Step(bool *pEnd, char *pError, int ErrorSize) override;
	bool ExecuteUpdate(int *pNumUpdated, char *pError, int ErrorSize) override;

	bool IsNull(int Col) override;
	float GetFloat(int Col) override;
	int GetInt(int Col) override;
	int64_t GetInt64(int Col) override;
	void GetString(int Col, char *pBuffer, int BufferSize) override;
	int GetBlob(int Col, unsigned char *pBuffer, int BufferSize) override;

	bool AddPoints(const char *pPlayer, int Points, char *pError, int ErrorSize) override;

private:
	// copy of config vars
	CPostgresqlConfig m_Config;

	PGconn *m_pConn = nullptr;

	// The current prepared statement and its bound parameters, sized by the
	// largest bound index. The parameters are executed lazily with
	// PQexecParams on the first Step()/ExecuteUpdate() call after they've
	// been bound.
	std::string m_Query;
	std::vector<std::string> m_vParamValues; // owning storage (also holds binary blobs)
	std::vector<bool> m_vParamNull;
	std::vector<int> m_vParamLengths;
	std::vector<int> m_vParamFormats; // 0 = text, 1 = binary
	std::vector<Oid> m_vParamTypes; // 0 = let the server infer the type
	bool m_NewQuery = false;

	// The result of the current query, buffered client-side. Step() advances
	// the row cursor over it.
	PGresult *m_pResult = nullptr;
	int m_CurRow = -1;
	int m_NumRows = 0;

	std::atomic_bool m_InUse;

	void EnsureNumParams(int NumParams);
	void SetParamText(int Idx, const char *pStr);
	void ClearResult();
	bool ConnectImpl(char *pError, int ErrorSize);
	PGconn *ConnectToDatabase(const char *pDatabase, char *pError, int ErrorSize);
	bool CreateDatabase(char *pError, int ErrorSize);
	bool ExecSimple(const char *pQuery, char *pError, int ErrorSize);
	bool Execute(char *pError, int ErrorSize);
};

CPostgresqlConnection::CPostgresqlConnection(CPostgresqlConfig Config) :
	IDbConnection(Config.m_aPrefix),
	m_Config(Config),
	m_InUse(false)
{
}

CPostgresqlConnection::~CPostgresqlConnection()
{
	ClearResult();
	if(m_pConn != nullptr)
		PQfinish(m_pConn);
}

void CPostgresqlConnection::Print(const char *pMode)
{
	log_info("server",
		"PostgreSQL-%s: DB: '%s' Prefix: '%s' User: '%s' IP: <{'%s'}> Port: %d",
		pMode, m_Config.m_aDatabase, GetPrefix(), m_Config.m_aUser, m_Config.m_aIp, m_Config.m_Port);
}

void CPostgresqlConnection::ToUnixTimestamp(const char *pTimestamp, char *aBuf, unsigned int BufferSize)
{
	// Cast to `timestamp` (without time zone) so that the stored value and
	// CURRENT_TIMESTAMP are measured on the same basis. Absolute values are
	// therefore offset by the local time zone, but the only consumer computes
	// a difference (now - stored), in which the offset cancels out.
	str_format(aBuf, BufferSize, "EXTRACT(EPOCH FROM (%s)::timestamp)::bigint", pTimestamp);
}

void CPostgresqlConnection::ClearResult()
{
	if(m_pResult != nullptr)
	{
		PQclear(m_pResult);
		m_pResult = nullptr;
	}
	m_CurRow = -1;
	m_NumRows = 0;
}

bool CPostgresqlConnection::Connect(char *pError, int ErrorSize)
{
	dbg_assert(!m_InUse.exchange(true), "Tried connecting while the connection is in use");
	if(!ConnectImpl(pError, ErrorSize))
	{
		m_InUse.store(false);
		return false;
	}
	return true;
}

PGconn *CPostgresqlConnection::ConnectToDatabase(const char *pDatabase, char *pError, int ErrorSize)
{
	char aPort[16];
	str_format(aPort, sizeof(aPort), "%d", m_Config.m_Port);
	const char *apKeys[] = {"host", "port", "dbname", "user", "password", "connect_timeout", "application_name", nullptr};
	const char *apValues[] = {m_Config.m_aIp, aPort, pDatabase, m_Config.m_aUser, m_Config.m_aPass, "60", "DDNet", nullptr};
	PGconn *pConn = PQconnectdbParams(apKeys, apValues, 0);
	if(pConn == nullptr)
	{
		str_copy(pError, "could not allocate PostgreSQL connection", ErrorSize);
		return nullptr;
	}
	if(PQstatus(pConn) != CONNECTION_OK)
	{
		str_copy(pError, PQerrorMessage(pConn), ErrorSize);
		PQfinish(pConn);
		return nullptr;
	}
	return pConn;
}

bool CPostgresqlConnection::CreateDatabase(char *pError, int ErrorSize)
{
	// PostgreSQL has no "CREATE DATABASE IF NOT EXISTS" and CREATE DATABASE
	// can't run inside a transaction, so connect to the default maintenance
	// database, create the target database there and ignore the error if it
	// already exists.
	PGconn *pMaintenance = ConnectToDatabase("postgres", pError, ErrorSize);
	if(pMaintenance == nullptr)
		return false;

	char aQuery[256];
	str_format(aQuery, sizeof(aQuery), "CREATE DATABASE \"%s\"", m_Config.m_aDatabase);
	PGresult *pResult = PQexec(pMaintenance, aQuery);
	bool Success = PQresultStatus(pResult) == PGRES_COMMAND_OK;
	if(!Success)
	{
		const char *pSqlState = PQresultErrorField(pResult, PG_DIAG_SQLSTATE);
		if(pSqlState != nullptr && str_comp(pSqlState, "42P04") == 0) // duplicate_database
			Success = true;
		else
			str_copy(pError, PQerrorMessage(pMaintenance), ErrorSize);
	}
	PQclear(pResult);
	PQfinish(pMaintenance);
	return Success;
}

bool CPostgresqlConnection::ExecSimple(const char *pQuery, char *pError, int ErrorSize)
{
	PGresult *pResult = PQexec(m_pConn, pQuery);
	const ExecStatusType Status = PQresultStatus(pResult);
	const bool Success = Status == PGRES_COMMAND_OK || Status == PGRES_TUPLES_OK;
	if(!Success)
		str_copy(pError, PQerrorMessage(m_pConn), ErrorSize);
	PQclear(pResult);
	return Success;
}

bool CPostgresqlConnection::ConnectImpl(char *pError, int ErrorSize)
{
	if(m_pConn != nullptr)
	{
		if(PQstatus(m_pConn) == CONNECTION_OK)
			return true;
		PQreset(m_pConn);
		if(PQstatus(m_pConn) == CONNECTION_OK)
			return true;
		PQfinish(m_pConn);
		m_pConn = nullptr;
	}

	if(m_Config.m_Setup)
	{
		if(!CreateDatabase(pError, ErrorSize))
			return false;
	}

	m_pConn = ConnectToDatabase(m_Config.m_aDatabase, pError, ErrorSize);
	if(m_pConn == nullptr)
		return false;

	if(m_Config.m_Setup)
	{
		char aBuf[1024];
		FormatCreateRace(aBuf, sizeof(aBuf), /* Backup */ false);
		if(!ExecSimple(aBuf, pError, ErrorSize))
			return false;
		FormatCreateTeamrace(aBuf, sizeof(aBuf), "BYTEA", /* Backup */ false);
		if(!ExecSimple(aBuf, pError, ErrorSize))
			return false;
		FormatCreateMaps(aBuf, sizeof(aBuf));
		if(!ExecSimple(aBuf, pError, ErrorSize))
			return false;
		FormatCreateSaves(aBuf, sizeof(aBuf), /* Backup */ false);
		if(!ExecSimple(aBuf, pError, ErrorSize))
			return false;
		FormatCreatePoints(aBuf, sizeof(aBuf));
		if(!ExecSimple(aBuf, pError, ErrorSize))
			return false;
		m_Config.m_Setup = false;
	}
	log_info("postgresql", "connection established");
	return true;
}

void CPostgresqlConnection::Disconnect()
{
	ClearResult();
	m_InUse.store(false);
}

bool CPostgresqlConnection::PrepareStatement(const char *pStmt, char *pError, int ErrorSize)
{
	(void)pError;
	(void)ErrorSize;
	// The statement is only parsed by the server when it's executed with
	// PQexecParams, so syntax errors are reported from Step()/ExecuteUpdate().
	m_Query = pStmt;
	m_vParamValues.clear();
	m_vParamNull.clear();
	m_vParamLengths.clear();
	m_vParamFormats.clear();
	m_vParamTypes.clear();
	ClearResult();
	m_NewQuery = true;
	return true;
}

void CPostgresqlConnection::EnsureNumParams(int NumParams)
{
	if((int)m_vParamValues.size() < NumParams)
	{
		m_vParamValues.resize(NumParams);
		m_vParamNull.resize(NumParams, true);
		m_vParamLengths.resize(NumParams, 0);
		m_vParamFormats.resize(NumParams, 0);
		m_vParamTypes.resize(NumParams, 0);
	}
}

void CPostgresqlConnection::SetParamText(int Idx, const char *pStr)
{
	dbg_assert(Idx >= 1, "Error binding parameter: index out of bounds: %d", Idx);
	EnsureNumParams(Idx);
	Idx -= 1;
	m_vParamValues[Idx] = pStr;
	m_vParamNull[Idx] = false;
	m_vParamLengths[Idx] = 0;
	m_vParamFormats[Idx] = 0;
	m_vParamTypes[Idx] = 0;
	m_NewQuery = true;
}

void CPostgresqlConnection::BindString(int Idx, const char *pString)
{
	SetParamText(Idx, pString);
}

void CPostgresqlConnection::BindBlob(int Idx, unsigned char *pBlob, int Size)
{
	dbg_assert(Idx >= 1, "Error in BindBlob: index out of bounds: %d", Idx);
	EnsureNumParams(Idx);
	Idx -= 1;
	m_vParamValues[Idx].assign((const char *)pBlob, Size);
	m_vParamNull[Idx] = false;
	m_vParamLengths[Idx] = Size;
	m_vParamFormats[Idx] = 1; // binary
	m_vParamTypes[Idx] = PG_BYTEA_OID;
	m_NewQuery = true;
}

void CPostgresqlConnection::BindInt(int Idx, int Value)
{
	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Value);
	SetParamText(Idx, aBuf);
}

void CPostgresqlConnection::BindInt64(int Idx, int64_t Value)
{
	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%" PRId64, Value);
	SetParamText(Idx, aBuf);
}

void CPostgresqlConnection::BindFloat(int Idx, float Value)
{
	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%.10g", Value);
	SetParamText(Idx, aBuf);
}

void CPostgresqlConnection::BindNull(int Idx)
{
	dbg_assert(Idx >= 1, "Error in BindNull: index out of bounds: %d", Idx);
	EnsureNumParams(Idx);
	Idx -= 1;
	m_vParamValues[Idx].clear();
	m_vParamNull[Idx] = true;
	m_vParamLengths[Idx] = 0;
	m_vParamFormats[Idx] = 0;
	m_vParamTypes[Idx] = 0;
	m_NewQuery = true;
}

bool CPostgresqlConnection::Execute(char *pError, int ErrorSize)
{
	ClearResult();
	const int NumParams = (int)m_vParamValues.size();
	std::vector<const char *> vpValues(NumParams);
	for(int i = 0; i < NumParams; i++)
		vpValues[i] = m_vParamNull[i] ? nullptr : m_vParamValues[i].data();

	// Request text-format results (last argument 0) so that all column types
	// can be parsed uniformly without knowing their wire representation.
	m_pResult = PQexecParams(
		m_pConn,
		m_Query.c_str(),
		NumParams,
		NumParams ? m_vParamTypes.data() : nullptr,
		NumParams ? vpValues.data() : nullptr,
		NumParams ? m_vParamLengths.data() : nullptr,
		NumParams ? m_vParamFormats.data() : nullptr,
		0);
	const ExecStatusType Status = PQresultStatus(m_pResult);
	if(Status != PGRES_TUPLES_OK && Status != PGRES_COMMAND_OK)
	{
		const char *pMsg = PQresultErrorMessage(m_pResult);
		if(pMsg[0] == '\0')
			pMsg = PQerrorMessage(m_pConn);
		str_copy(pError, pMsg, ErrorSize);
		ClearResult();
		return false;
	}
	return true;
}

bool CPostgresqlConnection::Step(bool *pEnd, char *pError, int ErrorSize)
{
	if(m_NewQuery)
	{
		m_NewQuery = false;
		if(!Execute(pError, ErrorSize))
			return false;
		m_NumRows = PQntuples(m_pResult);
		m_CurRow = -1;
	}
	m_CurRow++;
	*pEnd = m_CurRow >= m_NumRows;
	return true;
}

bool CPostgresqlConnection::ExecuteUpdate(int *pNumUpdated, char *pError, int ErrorSize)
{
	if(!m_NewQuery)
	{
		str_copy(pError, "tried to execute update without query", ErrorSize);
		return false;
	}
	m_NewQuery = false;
	if(!Execute(pError, ErrorSize))
		return false;
	const char *pTuples = PQcmdTuples(m_pResult);
	*pNumUpdated = pTuples[0] != '\0' ? str_toint(pTuples) : 0;
	return true;
}

bool CPostgresqlConnection::IsNull(int Col)
{
	return PQgetisnull(m_pResult, m_CurRow, Col - 1) != 0;
}

float CPostgresqlConnection::GetFloat(int Col)
{
	return str_tofloat(PQgetvalue(m_pResult, m_CurRow, Col - 1));
}

int CPostgresqlConnection::GetInt(int Col)
{
	return str_toint(PQgetvalue(m_pResult, m_CurRow, Col - 1));
}

int64_t CPostgresqlConnection::GetInt64(int Col)
{
	return str_toint64_base(PQgetvalue(m_pResult, m_CurRow, Col - 1));
}

void CPostgresqlConnection::GetString(int Col, char *pBuffer, int BufferSize)
{
	str_copy(pBuffer, PQgetvalue(m_pResult, m_CurRow, Col - 1), BufferSize);
}

int CPostgresqlConnection::GetBlob(int Col, unsigned char *pBuffer, int BufferSize)
{
	size_t UnescapedSize = 0;
	unsigned char *pUnescaped = PQunescapeBytea((const unsigned char *)PQgetvalue(m_pResult, m_CurRow, Col - 1), &UnescapedSize);
	if(pUnescaped == nullptr)
		return 0;
	const int Size = std::min((int)UnescapedSize, BufferSize);
	mem_copy(pBuffer, pUnescaped, Size);
	PQfreemem(pUnescaped);
	return Size;
}

const char *CPostgresqlConnection::MedianMapTime(char *pBuffer, int BufferSize) const
{
	str_format(pBuffer, BufferSize,
		"SELECT PERCENTILE_CONT(0.5) WITHIN GROUP (ORDER BY Time) "
		"FROM %s_race "
		"WHERE Map = l.Map",
		GetPrefix());
	return pBuffer;
}

bool CPostgresqlConnection::AddPoints(const char *pPlayer, int Points, char *pError, int ErrorSize)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"INSERT INTO %s_points(Name, Points) "
		"VALUES ($1, $2) "
		"ON CONFLICT(Name) DO UPDATE SET Points=%s_points.Points+$3",
		GetPrefix(), GetPrefix());
	if(!PrepareStatement(aBuf, pError, ErrorSize))
	{
		return false;
	}
	BindString(1, pPlayer);
	BindInt(2, Points);
	BindInt(3, Points);
	int NumUpdated;
	return ExecuteUpdate(&NumUpdated, pError, ErrorSize);
}

std::unique_ptr<IDbConnection> CreatePostgresqlConnection(CPostgresqlConfig Config)
{
	return std::make_unique<CPostgresqlConnection>(Config);
}

bool PostgresqlAvailable()
{
	return true;
}
#else
bool PostgresqlAvailable()
{
	return false;
}
std::unique_ptr<IDbConnection> CreatePostgresqlConnection(CPostgresqlConfig Config)
{
	return nullptr;
}
#endif
