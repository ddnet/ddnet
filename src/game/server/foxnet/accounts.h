#ifndef GAME_SERVER_FOXNET_ACCOUNTS_H
#define GAME_SERVER_FOXNET_ACCOUNTS_H

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/storage.h>
#include <optional>
#include <string>
#include <memory>
#include <vector>
#include <functional>

struct CAccResult;
class CDbConnectionPool;
class CGameContext;
class IDbConnection;
class IServer;
struct ISqlData;

enum
{
	ACC_MIN_USERNAME_LENGTH = 4,
	ACC_MAX_USERNAME_LENGTH = 32,
	ACC_MIN_PASSW_LENGTH = 6,
	ACC_MAX_PASSW_LENGTH = 128,

	ACC_FLAG_AUTOLOGIN = 1 << 0,
	ACC_FLAG_HIDE_COSMETICS = 1 << 1,
	ACC_FLAG_HIDE_POWERUPS = 1 << 2,
};

struct CAccountSession
{
	char m_Username[ACC_MAX_USERNAME_LENGTH] = "";
	uint64_t m_RegisterDate = 0;
	char m_Name[MAX_NAME_LENGTH] = "";
	char m_LastName[MAX_NAME_LENGTH] = "";
	char CurrentIp[128] = "";
	char LastIp[128] = "";
	bool m_LoggedIn = false;
	uint64_t m_LastLogin = 0;
	int m_Port = 0;
	int ClientId = -1;
	int64_t m_Flags = 0;
	int m_VoteMenuPage = 0;
	int64_t m_Playtime = 0; // Minutes
	int64_t m_Deaths = 0;
	int64_t m_Kills = 0;
	int64_t m_Level = 0;
	int64_t m_XP = 0;
	int64_t m_Money = 0;
	char m_Inventory[1028] = "";
	char m_LastActiveItems[1028] = ""; // correlates to m_Inventory, will load this on login

	int m_LoginTick = 0;
	bool m_Disabled = false;
};

struct CPendingAccResult
{
	std::shared_ptr<CAccResult> m_pRes; // shared with sql worker
	std::function<void(CAccResult &)> m_Callback; // executed on main thread
};

class CAccounts
{
	CGameContext *m_pGameServer;
	CDbConnectionPool *m_pPool;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	// Password hashing
	SHA256_DIGEST HashPassword(const char *pPassword);

	std::vector<CPendingAccResult> m_vPending; 
	void AddPending(const std::shared_ptr<CAccResult> &pRes, std::function<void(CAccResult &)> &&Cb);

public:
	// Pool-aware init. Pass the same pool used by CScore.
	void Init(CGameContext *pGameServer, CDbConnectionPool *pPool);

	void Tick();

	bool Register(int ClientId, const char *pUsername, const char *pPassword, const char *pPassword2);
	bool ChangePassword(int ClientId, const char *pOldPassword, const char *pNewPassword, const char *pNewPassword2); // unchanged (fire-and-forget write)

	void AutoLogin(int ClientId); // async
	bool ForceLogin(int ClientId, const char *pUsername, bool Silent = false, bool Auto = false); // async

	void Login(int ClientId, const char *pUsername, const char *pPassword); // async
	bool Logout(int ClientId); // immediate

	void OnLogin(int ClientId, const struct CAccResult &Res);
	void OnLogout(int ClientId, const CAccountSession AccInfo);

	void SaveAccountsInfo(int ClientId, const CAccountSession AccInfo);
	void DisableAccount(const char *pUsername, bool Disable);

	void LogoutAllAccountsPort(int Port);
	void ShowAccProfile(int ClientId, const char *pName); // async

	void SaveAllAccounts();

	void Top5(int ClientId, const char *pType, int Offset = 0); // async

	void SetPlayerName(int ClientId, const char *pName);
	void EditAccount(const char *pUsername, const char *pVariable, const char *pValue); // async

	// Returns XP needed for next level
	int NeededXP(int Level);
};

#endif // GAME_SERVER_FOXNET_ACCOUNTS_H