#ifndef ENGINE_SERVER_DATABASES_CONNECTION_H
#define ENGINE_SERVER_DATABASES_CONNECTION_H

#include "connection_pool.h"

#include <engine/shared/protocol.h>

#include <memory>
#include <optional>
#include <string>

enum
{
	// MAX_NAME_LENGTH includes the size with \0, which is not necessary in SQL
	MAX_NAME_LENGTH_SQL = MAX_NAME_LENGTH - 1,
};

// can hold one PreparedStatement with Results
class IDbConnection
{
public:
	IDbConnection(const char *pPrefix);
	virtual ~IDbConnection() = default;
	IDbConnection &operator=(const IDbConnection &) = delete;
	virtual void Print(const char *pMode) = 0;

	// returns the database prefix
	const char *GetPrefix() const { return m_aPrefix; }
	virtual const char *BinaryCollate() const = 0;
	// can be inserted into queries to convert a timestamp variable to the unix timestamp
	virtual void ToUnixTimestamp(const char *pTimestamp, char *aBuf, unsigned int BufferSize) = 0;
	// placeholder for the Idx-th (1-based) bound parameter, `?` for MySQL/SQLite, `$Idx` for PostgreSQL
	virtual std::string Placeholder(int Idx) const = 0;
	// placeholder for the Idx-th bound parameter as a utc timestamp: since MySQL automatically
	// converts timestamps to utc, meanwhile sqlite code has to explicitly convert before inserting
	// timestamps, NOTE: CURRENT_TIMESTAMP in SQLite is UTC by default and doesn't have to be converted
	virtual std::string InsertTimestampAsUtc(int Idx) const = 0;
	// case-insensitive `LIKE` operator including the placeholder for the Idx-th bound parameter,
	// used as `WHERE Map <LikeNocase(1)>`, e.g. `LIKE ? COLLATE NOCASE` or `ILIKE $1`
	virtual std::string LikeNocase(int Idx) const = 0;
	// syntax to insert a row into table or ignore if it already exists
	virtual const char *InsertIgnore() const = 0;
	// appended to an `InsertIgnore()` statement, for backends that express the
	// "ignore on conflict" as a suffix instead of a prefix (PostgreSQL)
	virtual const char *InsertIgnoreEnd() const { return ""; }
	// ORDER BY RANDOM()/RAND()
	virtual const char *Random() const = 0;
	// Get Median Map Time from l.Map
	virtual const char *MedianMapTime(char *pBuffer, int BufferSize) const = 0;
	virtual const char *False() const = 0;
	virtual const char *True() const = 0;

	// tries to allocate the connection from the pool established
	//
	// returns true on success
	virtual bool Connect(char *pError, int ErrorSize) = 0;
	// has to be called to return the connection back to the pool
	virtual void Disconnect() = 0;

	// groups all statements until CommitTransaction into a transaction. A
	// transaction that wasn't committed is rolled back on Disconnect.
	virtual bool BeginTransaction(char *pError, int ErrorSize) = 0;
	virtual bool CommitTransaction(char *pError, int ErrorSize) = 0;

	// Placeholder(Idx) for placeholders, connection has to be established, can overwrite previous prepared statements
	//
	// returns true on success
	virtual bool PrepareStatement(const char *pStmt, char *pError, int ErrorSize) = 0;

	// PrepareStatement has to be called beforehand,
	virtual void BindString(int Idx, const char *pString) = 0;
	virtual void BindBlob(int Idx, unsigned char *pBlob, int Size) = 0;
	virtual void BindInt(int Idx, int Value) = 0;
	virtual void BindInt64(int Idx, int64_t Value) = 0;
	virtual void BindFloat(int Idx, float Value) = 0;
	virtual void BindNull(int Idx) = 0;

	// Print expanded sql statement
	virtual void Print() = 0;

	// executes the query and returns if a result row exists and selects it
	// when called multiple times the next row is selected
	//
	// returns true on success
	virtual bool Step(bool *pEnd, char *pError, int ErrorSize) = 0;
	// executes the query and returns the number of rows affected by the update/insert/delete
	//
	// returns true on success
	virtual bool ExecuteUpdate(int *pNumUpdated, char *pError, int ErrorSize) = 0;

	virtual bool IsNull(int Col) = 0;
	virtual float GetFloat(int Col) = 0;
	virtual int GetInt(int Col) = 0;
	virtual int64_t GetInt64(int Col) = 0;
	// ensures that the string is null terminated
	virtual void GetString(int Col, char *pBuffer, int BufferSize) = 0;
	// returns number of bytes read into the buffer
	virtual int GetBlob(int Col, unsigned char *pBuffer, int BufferSize) = 0;
	std::optional<float> GetOptionalFloat(int Col);
	std::optional<int> GetOptionalInt(int Col);
	std::optional<int64_t> GetOptionalInt64(int Col);

	// SQL statements, that can't be abstracted, has side effects to the result
	virtual bool AddPoints(const char *pPlayer, int Points, char *pError, int ErrorSize) = 0;

private:
	char m_aPrefix[64];

protected:
	void FormatCreateRace(char *aBuf, unsigned int BufferSize, bool Backup) const;
	void FormatCreateTeamrace(char *aBuf, unsigned int BufferSize, const char *pIdType, bool Backup) const;
	void FormatCreateMaps(char *aBuf, unsigned int BufferSize) const;
	void FormatCreateSaves(char *aBuf, unsigned int BufferSize, bool Backup) const;
	void FormatCreatePoints(char *aBuf, unsigned int BufferSize) const;
};

bool MysqlAvailable();
int MysqlInit();
void MysqlUninit();

bool PostgresqlAvailable();

std::unique_ptr<IDbConnection> CreateSqliteConnection(const char *pFilename, bool Setup);
// Returns nullptr if MySQL support is not compiled in.
std::unique_ptr<IDbConnection> CreateMysqlConnection(CMysqlConfig Config);
// Returns nullptr if PostgreSQL support is not compiled in.
std::unique_ptr<IDbConnection> CreatePostgresqlConnection(CPostgresqlConfig Config);

#endif // ENGINE_SERVER_DATABASES_CONNECTION_H
