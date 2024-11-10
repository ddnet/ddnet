#include "connection.h"

#include <engine/server/databases/connection_pool.h>

#if defined(CONF_MYSQL)
#include <mysql.h>

#include <base/tl/threading.h>
#include <engine/console.h>

#include <atomic>
#include <memory>
#include <vector>

// MySQL >= 8.0.1 removed my_bool, 8.0.2 accidentally reintroduced it: https://bugs.mysql.com/bug.php?id=87337
#if !defined(LIBMARIADB) && MYSQL_VERSION_ID >= 80001 && MYSQL_VERSION_ID != 80002
typedef bool my_bool;
#endif

enum
{
	MYSQLSTATE_UNINITIALIZED,
	MYSQLSTATE_INITIALIZED,
	MYSQLSTATE_SHUTTINGDOWN,
};

std::atomic_int g_MysqlState = {MYSQLSTATE_UNINITIALIZED};
std::atomic_int g_MysqlNumConnections;

bool MysqlAvailable()
{
	return true;
}

int MysqlInit()
{
	dbg_assert(mysql_thread_safe(), "MySQL library without thread safety");
	dbg_assert(g_MysqlState == MYSQLSTATE_UNINITIALIZED, "double MySQL initialization");
	if(mysql_library_init(0, nullptr, nullptr))
	{
		return 1;
	}
	int Uninitialized = MYSQLSTATE_UNINITIALIZED;
	bool Swapped = g_MysqlState.compare_exchange_strong(Uninitialized, MYSQLSTATE_INITIALIZED);
	(void)Swapped;
	dbg_assert(Swapped, "MySQL double initialization");
	return 0;
}

void MysqlUninit()
{
	int Initialized = MYSQLSTATE_INITIALIZED;
	bool Swapped = g_MysqlState.compare_exchange_strong(Initialized, MYSQLSTATE_SHUTTINGDOWN);
	(void)Swapped;
	dbg_assert(Swapped, "double MySQL free or free without initialization");
	int Counter = g_MysqlNumConnections;
	if(Counter != 0)
	{
		dbg_msg("mysql", "can't deinitialize, connections remaining: %d", Counter);
		return;
	}
	mysql_library_end();
}

class CMysqlConnection : public IDbConnection
{
public:
	explicit CMysqlConnection(CMysqlConfig m_Config);
	~CMysqlConnection();
	void Print(IConsole *pConsole, const char *pMode) override;

	const char *BinaryCollate() const override { return "utf8mb4_bin"; }
	void ToUnixTimestamp(const char *pTimestamp, char *aBuf, unsigned int BufferSize) override;
	const char *InsertTimestampAsUtc() const override { return "?"; }
	const char *CollateNocase() const override { return "CONVERT(? USING utf8mb4) COLLATE utf8mb4_general_ci"; }
	const char *InsertIgnore() const override { return "INSERT IGNORE"; }
	const char *Random() const override { return "RAND()"; }
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
	class CStmtDeleter
	{
	public:
		void operator()(MYSQL_STMT *pStmt) const;
	};

	char m_aErrorDetail[128];
	void StoreErrorMysql(const char *pContext);
	void StoreErrorStmt(const char *pContext);
	bool ConnectImpl();
	bool PrepareAndExecuteStatement(const char *pStmt);
	//static void DeleteResult(MYSQL_RES *pResult);

	union UParameterExtra
	{
		int i;
		unsigned long ul;
		float f;
	};

	bool m_NewQuery = false;
	bool m_HaveConnection = false;
	MYSQL m_Mysql;
	std::unique_ptr<MYSQL_STMT, CStmtDeleter> m_pStmt = nullptr;
	std::vector<MYSQL_BIND> m_vStmtParameters;
	std::vector<UParameterExtra> m_vStmtParameterExtras;

	// copy of m_Config vars
	CMysqlConfig m_Config;

	std::atomic_bool m_InUse;
};

void CMysqlConnection::CStmtDeleter::operator()(MYSQL_STMT *pStmt) const
{
	mysql_stmt_close(pStmt);
}

CMysqlConnection::CMysqlConnection(CMysqlConfig Config) :
	IDbConnection(Config.m_aPrefix),
	m_Config(Config),
	m_InUse(false)
{
	g_MysqlNumConnections += 1;
	dbg_assert(g_MysqlState == MYSQLSTATE_INITIALIZED, "MySQL library not in initialized state");

	mem_zero(m_aErrorDetail, sizeof(m_aErrorDetail));
	mem_zero(&m_Mysql, sizeof(m_Mysql));
	mysql_init(&m_Mysql);
}

CMysqlConnection::~CMysqlConnection()
{
	mysql_close(&m_Mysql);
	g_MysqlNumConnections -= 1;
}

void CMysqlConnection::StoreErrorMysql(const char *pContext)
{
	str_format(m_aErrorDetail, sizeof(m_aErrorDetail), "(%s:mysql:%d): %s", pContext, mysql_errno(&m_Mysql), mysql_error(&m_Mysql));
}

void CMysqlConnection::StoreErrorStmt(const char *pContext)
{
	str_format(m_aErrorDetail, sizeof(m_aErrorDetail), "(%s:stmt:%d): %s", pContext, mysql_stmt_errno(m_pStmt.get()), mysql_stmt_error(m_pStmt.get()));
}

bool CMysqlConnection::PrepareAndExecuteStatement(const char *pStmt)
{
	if(mysql_stmt_prepare(m_pStmt.get(), pStmt, str_length(pStmt)))
	{
		StoreErrorStmt("prepare");
		return true;
	}
	if(mysql_stmt_execute(m_pStmt.get()))
	{
		StoreErrorStmt("execute");
		return true;
	}
	return false;
}

void CMysqlConnection::Print(IConsole *pConsole, const char *pMode)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"MySQL-%s: DB: '%s' Prefix: '%s' User: '%s' IP: <{'%s'}> Port: %d",
		pMode, m_Config.m_aDatabase, GetPrefix(), m_Config.m_aUser, m_Config.m_aIp, m_Config.m_Port);
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CMysqlConnection::ToUnixTimestamp(const char *pTimestamp, char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize, "UNIX_TIMESTAMP(%s)", pTimestamp);
}

bool CMysqlConnection::Connect(char *pError, int ErrorSize)
{
	if(m_InUse.exchange(true))
	{
		dbg_assert(0, "Tried connecting while the connection is in use");
	}

	m_NewQuery = true;
	if(ConnectImpl())
	{
		str_copy(pError, m_aErrorDetail, ErrorSize);
		m_InUse.store(false);
		return true;
	}
	return false;
}

bool CMysqlConnection::ConnectImpl()
{
	if(m_HaveConnection)
	{
		if(m_pStmt && mysql_stmt_free_result(m_pStmt.get()))
		{
			StoreErrorStmt("free_result");
			dbg_msg("mysql", "can't free last result %s", m_aErrorDetail);
		}
		if(!mysql_select_db(&m_Mysql, m_Config.m_aDatabase))
		{
			// Success.
			return false;
		}
		StoreErrorMysql("select_db");
		dbg_msg("mysql", "ping error, trying to reconnect %s", m_aErrorDetail);
		mysql_close(&m_Mysql);
		mem_zero(&m_Mysql, sizeof(m_Mysql));
		mysql_init(&m_Mysql);
	}

	m_pStmt = nullptr;
	unsigned int OptConnectTimeout = 60;
	unsigned int OptReadTimeout = 60;
	unsigned int OptWriteTimeout = 120;
	my_bool OptReconnect = true;
	mysql_options(&m_Mysql, MYSQL_OPT_CONNECT_TIMEOUT, &OptConnectTimeout);
	mysql_options(&m_Mysql, MYSQL_OPT_READ_TIMEOUT, &OptReadTimeout);
	mysql_options(&m_Mysql, MYSQL_OPT_WRITE_TIMEOUT, &OptWriteTimeout);
	mysql_options(&m_Mysql, MYSQL_OPT_RECONNECT, &OptReconnect);
	mysql_options(&m_Mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");
	if(m_Config.m_aBindaddr[0] != '\0')
	{
		mysql_options(&m_Mysql, MYSQL_OPT_BIND, m_Config.m_aBindaddr);
	}

	if(!mysql_real_connect(&m_Mysql, m_Config.m_aIp, m_Config.m_aUser, m_Config.m_aPass, nullptr, m_Config.m_Port, nullptr, CLIENT_IGNORE_SIGPIPE))
	{
		StoreErrorMysql("real_connect");
		return true;
	}
	m_HaveConnection = true;

	m_pStmt = std::unique_ptr<MYSQL_STMT, CStmtDeleter>(mysql_stmt_init(&m_Mysql));

	// Apparently MYSQL_SET_CHARSET_NAME is not enough
	if(PrepareAndExecuteStatement("SET CHARACTER SET utf8mb4"))
	{
		return true;
	}

	if(m_Config.m_Setup)
	{
		char aCreateDatabase[1024];
		// create database
		str_format(aCreateDatabase, sizeof(aCreateDatabase), "CREATE DATABASE IF NOT EXISTS %s CHARACTER SET utf8mb4", m_Config.m_aDatabase);
		if(PrepareAndExecuteStatement(aCreateDatabase))
		{
			return true;
		}
	}

	// Connect to specific database
	if(mysql_select_db(&m_Mysql, m_Config.m_aDatabase))
	{
		StoreErrorMysql("select_db");
		return true;
	}

	if(m_Config.m_Setup)
	{
		char aCreateRace[1024];
		char aCreateTeamrace[1024];
		char aCreateMaps[1024];
		char aCreateSaves[1024];
		char aCreatePoints[1024];
		FormatCreateRace(aCreateRace, sizeof(aCreateRace), /* Backup */ false);
		FormatCreateTeamrace(aCreateTeamrace, sizeof(aCreateTeamrace), "VARBINARY(16)", /* Backup */ false);
		FormatCreateMaps(aCreateMaps, sizeof(aCreateMaps));
		FormatCreateSaves(aCreateSaves, sizeof(aCreateSaves), /* Backup */ false);
		FormatCreatePoints(aCreatePoints, sizeof(aCreatePoints));

		if(PrepareAndExecuteStatement(aCreateRace) ||
			PrepareAndExecuteStatement(aCreateTeamrace) ||
			PrepareAndExecuteStatement(aCreateMaps) ||
			PrepareAndExecuteStatement(aCreateSaves) ||
			PrepareAndExecuteStatement(aCreatePoints))
		{
			return true;
		}
		m_Config.m_Setup = false;
	}
	dbg_msg("mysql", "connection established");
	return false;
}

void CMysqlConnection::Disconnect()
{
	m_InUse.store(false);
}

bool CMysqlConnection::PrepareStatement(const char *pStmt, char *pError, int ErrorSize)
{
	if(mysql_stmt_prepare(m_pStmt.get(), pStmt, str_length(pStmt)))
	{
		StoreErrorStmt("prepare");
		str_copy(pError, m_aErrorDetail, ErrorSize);
		return true;
	}
	m_NewQuery = true;
	unsigned NumParameters = mysql_stmt_param_count(m_pStmt.get());
	m_vStmtParameters.resize(NumParameters);
	m_vStmtParameterExtras.resize(NumParameters);
	if(NumParameters)
	{
		mem_zero(m_vStmtParameters.data(), sizeof(m_vStmtParameters[0]) * m_vStmtParameters.size());
		mem_zero(m_vStmtParameterExtras.data(), sizeof(m_vStmtParameterExtras[0]) * m_vStmtParameterExtras.size());
	}
	return false;
}

void CMysqlConnection::BindString(int Idx, const char *pString)
{
	m_NewQuery = true;
	Idx -= 1;
	dbg_assert(0 <= Idx && Idx < (int)m_vStmtParameters.size(), "index out of bounds");

	int Length = str_length(pString);
	m_vStmtParameterExtras[Idx].ul = Length;
	MYSQL_BIND *pParam = &m_vStmtParameters[Idx];
	pParam->buffer_type = MYSQL_TYPE_STRING;
	pParam->buffer = (void *)pString;
	pParam->buffer_length = Length + 1;
	pParam->length = &m_vStmtParameterExtras[Idx].ul;
	pParam->is_null = nullptr;
	pParam->is_unsigned = false;
	pParam->error = nullptr;
}

void CMysqlConnection::BindBlob(int Idx, unsigned char *pBlob, int Size)
{
	m_NewQuery = true;
	Idx -= 1;
	dbg_assert(0 <= Idx && Idx < (int)m_vStmtParameters.size(), "index out of bounds");

	m_vStmtParameterExtras[Idx].ul = Size;
	MYSQL_BIND *pParam = &m_vStmtParameters[Idx];
	pParam->buffer_type = MYSQL_TYPE_BLOB;
	pParam->buffer = pBlob;
	pParam->buffer_length = Size;
	pParam->length = &m_vStmtParameterExtras[Idx].ul;
	pParam->is_null = nullptr;
	pParam->is_unsigned = false;
	pParam->error = nullptr;
}

void CMysqlConnection::BindInt(int Idx, int Value)
{
	m_NewQuery = true;
	Idx -= 1;
	dbg_assert(0 <= Idx && Idx < (int)m_vStmtParameters.size(), "index out of bounds");

	m_vStmtParameterExtras[Idx].i = Value;
	MYSQL_BIND *pParam = &m_vStmtParameters[Idx];
	pParam->buffer_type = MYSQL_TYPE_LONG;
	pParam->buffer = &m_vStmtParameterExtras[Idx].i;
	pParam->buffer_length = sizeof(m_vStmtParameterExtras[Idx].i);
	pParam->length = nullptr;
	pParam->is_null = nullptr;
	pParam->is_unsigned = false;
	pParam->error = nullptr;
}

void CMysqlConnection::BindInt64(int Idx, int64_t Value)
{
	m_NewQuery = true;
	Idx -= 1;
	dbg_assert(0 <= Idx && Idx < (int)m_vStmtParameters.size(), "index out of bounds");

	m_vStmtParameterExtras[Idx].i = Value;
	MYSQL_BIND *pParam = &m_vStmtParameters[Idx];
	pParam->buffer_type = MYSQL_TYPE_LONGLONG;
	pParam->buffer = &m_vStmtParameterExtras[Idx].i;
	pParam->buffer_length = sizeof(m_vStmtParameterExtras[Idx].i);
	pParam->length = nullptr;
	pParam->is_null = nullptr;
	pParam->is_unsigned = false;
	pParam->error = nullptr;
}

void CMysqlConnection::BindFloat(int Idx, float Value)
{
	m_NewQuery = true;
	Idx -= 1;
	dbg_assert(0 <= Idx && Idx < (int)m_vStmtParameters.size(), "index out of bounds");

	m_vStmtParameterExtras[Idx].f = Value;
	MYSQL_BIND *pParam = &m_vStmtParameters[Idx];
	pParam->buffer_type = MYSQL_TYPE_FLOAT;
	pParam->buffer = &m_vStmtParameterExtras[Idx].f;
	pParam->buffer_length = sizeof(m_vStmtParameterExtras[Idx].i);
	pParam->length = nullptr;
	pParam->is_null = nullptr;
	pParam->is_unsigned = false;
	pParam->error = nullptr;
}

void CMysqlConnection::BindNull(int Idx)
{
	m_NewQuery = true;
	Idx -= 1;
	dbg_assert(0 <= Idx && Idx < (int)m_vStmtParameters.size(), "index out of bounds");

	MYSQL_BIND *pParam = &m_vStmtParameters[Idx];
	pParam->buffer_type = MYSQL_TYPE_NULL;
	pParam->buffer = nullptr;
	pParam->buffer_length = 0;
	pParam->length = nullptr;
	pParam->is_null = nullptr;
	pParam->is_unsigned = false;
	pParam->error = nullptr;
}

bool CMysqlConnection::Step(bool *pEnd, char *pError, int ErrorSize)
{
	if(m_NewQuery)
	{
		m_NewQuery = false;
		if(mysql_stmt_bind_param(m_pStmt.get(), m_vStmtParameters.data()))
		{
			StoreErrorStmt("bind_param");
			str_copy(pError, m_aErrorDetail, ErrorSize);
			return true;
		}
		if(mysql_stmt_execute(m_pStmt.get()))
		{
			StoreErrorStmt("execute");
			str_copy(pError, m_aErrorDetail, ErrorSize);
			return true;
		}
	}
	int Result = mysql_stmt_fetch(m_pStmt.get());
	if(Result == 1)
	{
		StoreErrorStmt("fetch");
		str_copy(pError, m_aErrorDetail, ErrorSize);
		return true;
	}
	*pEnd = (Result == MYSQL_NO_DATA);
	// `Result` is now either `MYSQL_DATA_TRUNCATED` (which we ignore, we
	// fetch our columns in a different way) or `0` aka success.
	return false;
}

bool CMysqlConnection::ExecuteUpdate(int *pNumUpdated, char *pError, int ErrorSize)
{
	if(m_NewQuery)
	{
		m_NewQuery = false;
		if(mysql_stmt_bind_param(m_pStmt.get(), m_vStmtParameters.data()))
		{
			StoreErrorStmt("bind_param");
			str_copy(pError, m_aErrorDetail, ErrorSize);
			return true;
		}
		if(mysql_stmt_execute(m_pStmt.get()))
		{
			StoreErrorStmt("execute");
			str_copy(pError, m_aErrorDetail, ErrorSize);
			return true;
		}
		*pNumUpdated = mysql_stmt_affected_rows(m_pStmt.get());
		return false;
	}
	str_copy(pError, "tried to execute update without query", ErrorSize);
	return true;
}

bool CMysqlConnection::IsNull(int Col)
{
	Col -= 1;

	MYSQL_BIND Bind;
	my_bool IsNull;
	mem_zero(&Bind, sizeof(Bind));
	Bind.buffer_type = MYSQL_TYPE_NULL;
	Bind.buffer = nullptr;
	Bind.buffer_length = 0;
	Bind.length = nullptr;
	Bind.is_null = &IsNull;
	Bind.is_unsigned = false;
	Bind.error = nullptr;
	if(mysql_stmt_fetch_column(m_pStmt.get(), &Bind, Col, 0))
	{
		StoreErrorStmt("fetch_column:null");
		dbg_msg("mysql", "error fetching column %s", m_aErrorDetail);
		dbg_assert(0, "error in IsNull");
	}
	return IsNull;
}

float CMysqlConnection::GetFloat(int Col)
{
	Col -= 1;

	MYSQL_BIND Bind;
	float Value;
	my_bool IsNull;
	mem_zero(&Bind, sizeof(Bind));
	Bind.buffer_type = MYSQL_TYPE_FLOAT;
	Bind.buffer = &Value;
	Bind.buffer_length = sizeof(Value);
	Bind.length = nullptr;
	Bind.is_null = &IsNull;
	Bind.is_unsigned = false;
	Bind.error = nullptr;
	if(mysql_stmt_fetch_column(m_pStmt.get(), &Bind, Col, 0))
	{
		StoreErrorStmt("fetch_column:float");
		dbg_msg("mysql", "error fetching column %s", m_aErrorDetail);
		dbg_assert(0, "error in GetFloat");
	}
	if(IsNull)
	{
		dbg_assert(0, "error getting float: NULL");
	}
	return Value;
}

int CMysqlConnection::GetInt(int Col)
{
	Col -= 1;

	MYSQL_BIND Bind;
	int Value;
	my_bool IsNull;
	mem_zero(&Bind, sizeof(Bind));
	Bind.buffer_type = MYSQL_TYPE_LONG;
	Bind.buffer = &Value;
	Bind.buffer_length = sizeof(Value);
	Bind.length = nullptr;
	Bind.is_null = &IsNull;
	Bind.is_unsigned = false;
	Bind.error = nullptr;
	if(mysql_stmt_fetch_column(m_pStmt.get(), &Bind, Col, 0))
	{
		StoreErrorStmt("fetch_column:int");
		dbg_msg("mysql", "error fetching column %s", m_aErrorDetail);
		dbg_assert(0, "error in GetInt");
	}
	if(IsNull)
	{
		dbg_assert(0, "error getting int: NULL");
	}
	return Value;
}

int64_t CMysqlConnection::GetInt64(int Col)
{
	Col -= 1;

	MYSQL_BIND Bind;
	int64_t Value;
	my_bool IsNull;
	mem_zero(&Bind, sizeof(Bind));
	Bind.buffer_type = MYSQL_TYPE_LONGLONG;
	Bind.buffer = &Value;
	Bind.buffer_length = sizeof(Value);
	Bind.length = nullptr;
	Bind.is_null = &IsNull;
	Bind.is_unsigned = false;
	Bind.error = nullptr;
	if(mysql_stmt_fetch_column(m_pStmt.get(), &Bind, Col, 0))
	{
		StoreErrorStmt("fetch_column:int64");
		dbg_msg("mysql", "error fetching column %s", m_aErrorDetail);
		dbg_assert(0, "error in GetInt64");
	}
	if(IsNull)
	{
		dbg_assert(0, "error getting int: NULL");
	}
	return Value;
}

void CMysqlConnection::GetString(int Col, char *pBuffer, int BufferSize)
{
	Col -= 1;

	for(int i = 0; i < BufferSize; i++)
	{
		pBuffer[i] = '\0';
	}

	MYSQL_BIND Bind;
	unsigned long Length;
	my_bool IsNull;
	my_bool Error;
	mem_zero(&Bind, sizeof(Bind));
	Bind.buffer_type = MYSQL_TYPE_STRING;
	Bind.buffer = pBuffer;
	// leave one character for null-termination
	Bind.buffer_length = BufferSize - 1;
	Bind.length = &Length;
	Bind.is_null = &IsNull;
	Bind.is_unsigned = false;
	Bind.error = &Error;
	if(mysql_stmt_fetch_column(m_pStmt.get(), &Bind, Col, 0))
	{
		StoreErrorStmt("fetch_column:string");
		dbg_msg("mysql", "error fetching column %s", m_aErrorDetail);
		dbg_assert(0, "error in GetString");
	}
	if(IsNull)
	{
		dbg_assert(0, "error getting string: NULL");
	}
	if(Error)
	{
		dbg_assert(0, "error getting string: truncation occurred");
	}
}

int CMysqlConnection::GetBlob(int Col, unsigned char *pBuffer, int BufferSize)
{
	Col -= 1;

	MYSQL_BIND Bind;
	unsigned long Length;
	my_bool IsNull;
	my_bool Error;
	mem_zero(&Bind, sizeof(Bind));
	Bind.buffer_type = MYSQL_TYPE_BLOB;
	Bind.buffer = pBuffer;
	Bind.buffer_length = BufferSize;
	Bind.length = &Length;
	Bind.is_null = &IsNull;
	Bind.is_unsigned = false;
	Bind.error = &Error;
	if(mysql_stmt_fetch_column(m_pStmt.get(), &Bind, Col, 0))
	{
		StoreErrorStmt("fetch_column:blob");
		dbg_msg("mysql", "error fetching column %s", m_aErrorDetail);
		dbg_assert(0, "error in GetBlob");
	}
	if(IsNull)
	{
		dbg_assert(0, "error getting blob: NULL");
	}
	if(Error)
	{
		dbg_assert(0, "error getting blob: truncation occurred");
	}
	return Length;
}

const char *CMysqlConnection::MedianMapTime(char *pBuffer, int BufferSize) const
{
	str_format(pBuffer, BufferSize,
		"SELECT MEDIAN(Time) "
		"OVER (PARTITION BY Map) "
		"FROM %s_race "
		"WHERE Map = l.Map "
		"LIMIT 1",
		GetPrefix());
	return pBuffer;
}

bool CMysqlConnection::AddPoints(const char *pPlayer, int Points, char *pError, int ErrorSize)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"INSERT INTO %s_points(Name, Points) "
		"VALUES (?, ?) "
		"ON DUPLICATE KEY UPDATE Points=Points+?",
		GetPrefix());
	if(PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	BindString(1, pPlayer);
	BindInt(2, Points);
	BindInt(3, Points);
	int NumUpdated;
	return ExecuteUpdate(&NumUpdated, pError, ErrorSize);
}

std::unique_ptr<IDbConnection> CreateMysqlConnection(CMysqlConfig Config)
{
	return std::make_unique<CMysqlConnection>(Config);
}
#else
bool MysqlAvailable()
{
	return false;
}
int MysqlInit()
{
	return 0;
}
void MysqlUninit()
{
}
std::unique_ptr<IDbConnection> CreateMysqlConnection(CMysqlConfig Config)
{
	return nullptr;
}
#endif
