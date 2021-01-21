#ifndef ENGINE_SERVER_DATABASES_SQLITE_H
#define ENGINE_SERVER_DATABASES_SQLITE_H

#include "connection.h"
#include <atomic>

struct sqlite3;
struct sqlite3_stmt;

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

	virtual Status Connect();
	virtual void Disconnect();

	virtual void PrepareStatement(const char *pStmt);

	virtual void BindString(int Idx, const char *pString);
	virtual void BindBlob(int Idx, unsigned char *pBlob, int Size);
	virtual void BindInt(int Idx, int Value);
	virtual void BindFloat(int Idx, float Value);

	virtual void Print();
	virtual bool Step();
	virtual int ExecuteUpdate();

	virtual bool IsNull(int Col) const;
	virtual float GetFloat(int Col) const;
	virtual int GetInt(int Col) const;
	virtual void GetString(int Col, char *pBuffer, int BufferSize) const;
	// passing a negative buffer size is undefined behavior
	virtual int GetBlob(int Col, unsigned char *pBuffer, int BufferSize) const;

	virtual void AddPoints(const char *pPlayer, int Points);

private:
	// copy of config vars
	char m_aFilename[512];
	bool m_Setup;

	sqlite3 *m_pDb;
	sqlite3_stmt *m_pStmt;
	bool m_Done; // no more rows available for Step
	// returns true, if the query succeeded
	bool Execute(const char *pQuery);

	void ExceptionOnError(int Result);

	std::atomic_bool m_InUse;
};

#endif // ENGINE_SERVER_DATABASES_SQLITE_H
