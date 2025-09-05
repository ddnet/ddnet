#ifndef GAME_SERVER_FOXNET_ACCOUNTS_H
#define GAME_SERVER_FOXNET_ACCOUNTS_H

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/storage.h>
#include <optional>
#include <string>
#include <memory>

class CDbConnectionPool;
class CGameContext;
class IDbConnection;
class IServer;
struct ISqlData;

enum
{
	ACC_MAX_USERNAME_LENGTH = 32,
	ACC_MIN_USERNAME_LENGTH = 4,
	ACC_MAX_PASSW_LENGTH = 128,
	ACC_MIN_PASSW_LENGTH = 6,

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

class CAccounts
{
	CGameContext *m_pGameServer;
	CDbConnectionPool *m_pPool;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	// Wait helper for simple synchronous flows (register/login).
	// Returns true if completed and successful.
	bool WaitForResult(const std::shared_ptr<struct ISqlResult> &pRes, const char *pOpName, int TimeoutMs = 2000);

	// Password hashing
	SHA256_DIGEST HashPassword(const char *pPassword);

public:
	// Pool-aware init. Pass the same pool used by CScore.
	void Init(CGameContext *pGameServer, CDbConnectionPool *pPool);

	// Account operations
	bool Register(int ClientId, const char *pUsername, const char *pPassword, const char *pPassword2);
	bool ChangePassword(int ClientId, const char *pOldPassword, const char *pNewPassword, const char *pNewPassword2);

	void AutoLogin(int ClientId);
	bool ForceLogin(int ClientId, const char *pUsername);

	bool Login(int ClientId, const char *pUsername, const char *pPassword);
	bool Logout(int ClientId);

	// In-memory session -> DB persist helpers
	void OnLogin(int ClientId, const struct CAccResult &Res);
	void OnLogout(int ClientId, const CAccountSession AccInfo);

	void SaveAccountsInfo(int ClientId, const CAccountSession AccInfo);
	void DisableAccount(const char *pUsername, bool Disable);

	void LogoutAllAccountsPort(int Port);
	void ShowAccProfile(int ClientId, const char *pName);

	void SaveAllAccounts();

	void Top5(int ClientId, const char *pType, int Offset = 0);

	std::optional<CAccountSession> GetAccount(const char *pUsername);
	std::optional<CAccountSession> GetAccountCurName(const char *pLastName);

	CAccountSession GetAccount(int ClientId);

	void SetPlayerName(int ClientId, const char *pName);
	void EditAccount(const char *pUsername, const char *pVariable, const char *pValue);

	// Returns 0 if not logged in and the current port if logged in
	int IsAccountLoggedIn(const char *pUsername);

	int NeededXP(int Level);
};

#endif // GAME_SERVER_FOXNET_ACCOUNTS_H