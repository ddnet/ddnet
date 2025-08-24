#ifndef GAME_SERVER_FOXNET_ACCOUNTS_H
#define GAME_SERVER_FOXNET_ACCOUNTS_H

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/storage.h>
#include <optional>
#include <string>

#include <sqlite3.h>

class CGameContext;
class IServer;

enum
{
	ACC_MAX_USERNAME_LENGTH = 32,
	ACC_MIN_USERNAME_LENGTH = 4,
	ACC_MAX_PASSW_LENGTH = 128,
	ACC_MIN_PASSW_LENGTH = 6,

	ACC_FLAG_AUTOLOGIN = 1 << 0,
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
	uint64_t m_Flags = 0;
	uint64_t m_VoteMenuFlags = 0;
	uint64_t m_Playtime = 0; // Minutes
	uint64_t m_Deaths = 0;
	uint64_t m_Kills = 0;
	uint64_t m_Level = 0;
	uint64_t m_XP = 0;
	uint64_t m_Money = 0;
	char m_Inventory[1028] = "";
};

class CAccounts
{
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	sqlite3 *m_AccDatabase;

public:
	void Init(CGameContext *pGameServer);
	void OnShutdown();

	SHA256_DIGEST HashPassword(const char *pPassword);

	bool Register(int ClientId, const char *pUsername, const char *pPassword, const char *pPassword2);

	bool ChangePassword(int ClientId, const char *pOldPassword, const char *pNewPassword, const char *pNewPassword2);

	void AutoLogin(int ClientId);
	bool ForceLogin(int ClientId, const char *pUsername);

	bool Login(int ClientId, const char *pUsername, const char *pPassword);
	bool Logout(int ClientId);

	void OnLogin(int ClientId, const char *pUsername);
	void OnLogout(int ClientId, const CAccountSession AccInfo);

	void SaveAccountsInfo(int ClientId, const CAccountSession AccInfo);

	void LogoutAllAccountsPort(int Port);
	void ShowAccProfile(int ClientId, const char *pName);

	void SaveAllAccounts();

	std::optional<CAccountSession> GetAccount(const char *pUsername);
	std::optional<CAccountSession> GetAccountCurName(const char *pLastName);

	CAccountSession GetAccount(int ClientId);

	void SetPlayerName(int ClientId, const char *pName);

	// std::optional<CAccountSession> AccountInfoUsername(const char *pUsername);
};

#endif // GAME_SERVER_FOXNET_ACCOUNTS_H