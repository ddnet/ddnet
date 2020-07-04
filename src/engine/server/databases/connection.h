#ifndef ENGINE_SERVER_DATABASES_CONNECTION_H
#define ENGINE_SERVER_DATABASES_CONNECTION_H

#include <base/system.h>

// can hold one PreparedStatement with Results
class IDbConnection
{
public:
	IDbConnection(const char *pPrefix)
	{
		str_copy(m_aPrefix, pPrefix, sizeof(m_aPrefix));
	}
	virtual ~IDbConnection() {}
	IDbConnection& operator=(const IDbConnection&) = delete;

	// copies the credentials, not the active connection
	virtual IDbConnection *Copy() = 0;

	// returns the database prefix
	const char *GetPrefix() { return m_aPrefix; }

	enum Status
	{
		IN_USE,
		SUCCESS,
		ERROR,
	};
	// tries to allocate the connection from the pool established
	virtual Status Connect() = 0;
	// has to be called to return the connection back to the pool
	virtual void Disconnect() = 0;

	// get exclusive read/write access to the database
	virtual void Lock(const char *pTable) = 0;
	virtual void Unlock() = 0;

	// ? for Placeholders, connection has to be established, can overwrite previous prepared statements
	virtual void PrepareStatement(const char *pStmt) = 0;

	// PrepareStatement has to be called beforehand,
	virtual void BindString(int Idx, const char *pString) = 0;
	virtual void BindInt(int Idx, int Value) = 0;

	// executes the query and returns if a result row exists and selects it
	// when called multiple times the next row is selected
	virtual bool Step() = 0;

	virtual bool IsNull(int Col) const = 0;
	virtual float GetFloat(int Col) const = 0;
	virtual int GetInt(int Col) const = 0;
	// ensures that the string is null terminated
	virtual void GetString(int Col, char *pBuffer, int BufferSize) const = 0;
	// returns number of bytes read into the buffer
	virtual int GetBlob(int Col, unsigned char *pBuffer, int BufferSize) const = 0;

protected:
	char m_aPrefix[64];

	static constexpr const char *m_pCreateRace =
		"CREATE TABLE IF NOT EXISTS %s_race ("
			"Map VARCHAR(128) BINARY NOT NULL, "
			"Name VARCHAR(%d) BINARY NOT NULL, "
			"Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
			"Time FLOAT DEFAULT 0, "
			"Server CHAR(4), "
			"cp1 FLOAT DEFAULT 0, cp2 FLOAT DEFAULT 0, cp3 FLOAT DEFAULT 0, "
			"cp4 FLOAT DEFAULT 0, cp5 FLOAT DEFAULT 0, cp6 FLOAT DEFAULT 0, "
			"cp7 FLOAT DEFAULT 0, cp8 FLOAT DEFAULT 0, cp9 FLOAT DEFAULT 0, "
			"cp10 FLOAT DEFAULT 0, cp11 FLOAT DEFAULT 0, cp12 FLOAT DEFAULT 0, "
			"cp13 FLOAT DEFAULT 0, cp14 FLOAT DEFAULT 0, cp15 FLOAT DEFAULT 0, "
			"cp16 FLOAT DEFAULT 0, cp17 FLOAT DEFAULT 0, cp18 FLOAT DEFAULT 0, "
			"cp19 FLOAT DEFAULT 0, cp20 FLOAT DEFAULT 0, cp21 FLOAT DEFAULT 0, "
			"cp22 FLOAT DEFAULT 0, cp23 FLOAT DEFAULT 0, cp24 FLOAT DEFAULT 0, "
			"cp25 FLOAT DEFAULT 0, "
			"GameID VARCHAR(64), "
			"DDNet7 BOOL DEFAULT FALSE, "
			"KEY (Map, Name)"
		") CHARACTER SET utf8mb4;";
	static constexpr const char *m_pCreateTeamrace =
		"CREATE TABLE IF NOT EXISTS %s_teamrace ("
			"Map VARCHAR(128) BINARY NOT NULL, "
			"Name VARCHAR(%d) BINARY NOT NULL, "
			"Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
			"Time FLOAT DEFAULT 0, "
			"ID VARBINARY(16) NOT NULL, "
			"GameID VARCHAR(64), "
			"DDNet7 BOOL DEFAULT FALSE, "
			"KEY Map (Map)"
		") CHARACTER SET utf8mb4;";
	static constexpr const char *m_pCreateMaps =
		"CREATE TABLE IF NOT EXISTS %s_maps ("
			"Map VARCHAR(128) BINARY NOT NULL, "
			"Server VARCHAR(32) BINARY NOT NULL, "
			"Mapper VARCHAR(128) BINARY NOT NULL, "
			"Points INT DEFAULT 0, "
			"Stars INT DEFAULT 0, "
			"Timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
			"UNIQUE KEY Map (Map)"
		") CHARACTER SET utf8mb4;";
	static constexpr const char *m_pCreateSaves =
		"CREATE TABLE IF NOT EXISTS %s_saves ("
			"Savegame TEXT CHARACTER SET utf8mb4 BINARY NOT NULL, "
			"Map VARCHAR(128) BINARY NOT NULL, "
			"Code VARCHAR(128) BINARY NOT NULL, "
			"Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
			"Server CHAR(4), "
			"DDNet7 BOOL DEFAULT FALSE, "
			"SaveID VARCHAR(36) DEFAULT NULL, "
			"UNIQUE KEY (Map, Code)"
		") CHARACTER SET utf8mb4;";
	static constexpr const char *m_pCreatePoints =
		"CREATE TABLE IF NOT EXISTS %s_points ("
			"Name VARCHAR(%d) BINARY NOT NULL, "
			"Points INT DEFAULT 0, "
			"UNIQUE KEY Name (Name)"
		") CHARACTER SET utf8mb4;";
};

#endif // ENGINE_SERVER_DATABASES_CONNECTION_H
