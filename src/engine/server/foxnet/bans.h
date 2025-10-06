#ifndef ENGINE_SHARED_FOXNET_BANS_H
#define ENGINE_SHARED_FOXNET_BANS_H

#include <base/system.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>

class IConsole;
class IStorage;
class CServer;

struct CBanIp : ISqlData
{
	CBanIp() :
		ISqlData(nullptr) {}
	char m_aIP[NETADDR_MAXSTRSIZE] = "";
	int64_t m_EndTimestamp = 0;
	char m_aIssuer[MAX_NAME_LENGTH] = "";
	char m_aPlayerName[MAX_NAME_LENGTH] = "";
	char m_aReason[128] = "";
	class CServer *m_pServer = nullptr;
};

struct CCheckBanned : ISqlData
{
	CCheckBanned() :
		ISqlData(nullptr) {}
	char m_aIP[NETADDR_MAXSTRSIZE] = "";
	class CServer *m_pServer = nullptr;
};

class CFoxNetBans
{
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;
	class CServer *m_pServer;
 
public:
	void Init(IConsole *pConsole, IStorage *pStorage, CServer *pServer);

	void CheckBanned(const NETADDR *pAddr);
	static bool CheckBannedIp(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize);

	void Ban(const NETADDR *pAddr, int64_t Duration, const char *pIssuer, const char *pPlayerName, const char *pReason);
	static bool BanIp(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize);

	void Unban(const NETADDR *pAddr);
	static bool UnbanIp(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize);
};

#endif // ENGINE_SHARED_FOXNET_BANS_H