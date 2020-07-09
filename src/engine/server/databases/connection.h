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

private:
	char m_aPrefix[64];

protected:
	void FormatCreateRace(char *aBuf, unsigned int BufferSize);
	void FormatCreateTeamrace(char *aBuf, unsigned int BufferSize);
	void FormatCreateMaps(char *aBuf, unsigned int BufferSize);
	void FormatCreateSaves(char *aBuf, unsigned int BufferSize);
	void FormatCreatePoints(char *aBuf, unsigned int BufferSize);
};

#endif // ENGINE_SERVER_DATABASES_CONNECTION_H
