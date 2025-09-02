#ifndef GAME_SERVER_FOXNET_ACCOUNTWORKER_H
#define GAME_SERVER_FOXNET_ACCOUNTWORKER_H

#include <engine/server/databases/connection_pool.h>
#include <engine/shared/protocol.h>
#include "accounts.h"

class IDbConnection;
class IGameController;

struct CAccResult : ISqlResult
{
	bool m_Found = false;
	char m_Username[ACC_MAX_USERNAME_LENGTH]{};
	char m_PlayerName[MAX_NAME_LENGTH]{};
	char m_LastPlayerName[MAX_NAME_LENGTH]{};
	char m_CurrentIP[46]{};
	char m_LastIP[46]{};
	int64_t m_RegisterDate = 0;
	int m_LoggedIn = 0;
	int64_t m_LastLogin = 0;
	int m_Port = 0;
	int m_ClientId = -1;
	int64_t m_Flags = 0;
	int m_VoteMenuPage = 0;
	int64_t m_Playtime = 0;
	int64_t m_Deaths = 0;
	int64_t m_Kills = 0;
	int64_t m_Level = 0;
	int64_t m_XP = 0;
	int64_t m_Money = 0;
	char m_Inventory[1028]{};
	char m_LastActiveItems[1028]{};
};

struct CAccRegisterRequest : ISqlData
{
	CAccRegisterRequest(std::shared_ptr<CAccResult> pRes) :
		ISqlData(std::move(pRes)) {}
	char m_Username[ACC_MAX_USERNAME_LENGTH]{};
	char m_PasswordHash[ACC_MAX_PASSW_LENGTH]{};
	int64_t m_RegisterDate = 0;
};

struct CAccLoginRequest : ISqlData
{
	CAccLoginRequest(std::shared_ptr<CAccResult> pRes) :
		ISqlData(std::move(pRes)) {}
	char m_Username[ACC_MAX_USERNAME_LENGTH]{};
	char m_PasswordHash[256]{};
};

struct CAccSelectByUser : ISqlData
{
	CAccSelectByUser(std::shared_ptr<ISqlResult> pRes) :
		ISqlData(std::move(pRes)) {}
	char m_Username[ACC_MAX_USERNAME_LENGTH]{};
};

struct CAccSelectByLastName : ISqlData
{
	CAccSelectByLastName(std::shared_ptr<ISqlResult> pRes) :
		ISqlData(std::move(pRes)) {}
	char m_LastPlayerName[MAX_NAME_LENGTH]{};
};

struct CAccSelectPortByUser : ISqlData
{
	CAccSelectPortByUser(std::shared_ptr<ISqlResult> pRes) :
		ISqlData(std::move(pRes)) {}
	char m_Username[ACC_MAX_USERNAME_LENGTH]{};
};

struct CAccUpdLoginState : ISqlData
{
	CAccUpdLoginState() :
		ISqlData(nullptr) {}
	char m_Username[ACC_MAX_USERNAME_LENGTH]{};
	char m_PlayerName[MAX_NAME_LENGTH]{};
	char m_CurrentIP[46]{};
	int64_t m_LastLogin = 0;
	int m_Port = 0;
	int m_ClientId = -1;
};

struct CAccUpdLogoutState : ISqlData
{
	CAccUpdLogoutState() :
		ISqlData(nullptr) {}
	char m_Username[ACC_MAX_USERNAME_LENGTH]{};
	int64_t m_Flags = 0;
	int m_VoteMenuPage = 0;
	int64_t m_Playtime = 0;
	int64_t m_Deaths = 0;
	int64_t m_Kills = 0;
	int64_t m_Level = 0;
	int64_t m_XP = 0;
	int64_t m_Money = 0;
	char m_Inventory[1028]{};
	char m_LastActiveItems[1028]{};
};

struct CAccSaveInfo : ISqlData
{
	CAccSaveInfo() :
		ISqlData(nullptr) {}
	char m_Username[ACC_MAX_USERNAME_LENGTH]{};
	int64_t m_Flags = 0;
	int m_VoteMenuPage = 0;
	int64_t m_Playtime = 0;
	int64_t m_Deaths = 0;
	int64_t m_Kills = 0;
	int64_t m_Level = 0;
	int64_t m_XP = 0;
	int64_t m_Money = 0;
	char m_Inventory[1028]{};
	char m_LastActiveItems[1028]{};
};

struct CAccSetNameReq : ISqlData
{
	CAccSetNameReq() :
		ISqlData(nullptr) {}
	char m_Username[ACC_MAX_USERNAME_LENGTH]{};
	char m_NewPlayerName[MAX_NAME_LENGTH]{};
};

struct CAccShowTop5 : ISqlData
{
	CAccShowTop5() :
		ISqlData(nullptr) {}
	int m_ClientId;
	char m_Type[16];
	int m_Offset = 0;
	CGameContext *m_pGameServer;
};

struct CAccountsWorker
{
	static bool Register(IDbConnection *pSql, const ISqlData *pData, Write w, char *pError, int ErrorSize);
	static bool Login(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize);
	static bool SelectByUsername(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize);
	static bool SelectByLastPlayerName(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize);
	static bool SelectPortByUsername(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize);
	static bool UpdateLoginState(IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize);
	static bool UpdateLogoutState(IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize);
	static bool SetPlayerName(IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize);
	static bool SaveInfo(IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize);
	static bool ShowTop5(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize);
};

#endif