#include "accounts.h"
#include "game/server/gamecontext.h"
#include "accountworker.h"

#include <base/hash.h>
#include <base/hash_ctxt.h>
#include <base/log.h>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <thread>

#include <engine/server.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

// Local worker request/handlers for updates and simple selects not covered by accounts_worker

IServer *CAccounts::Server() const { return GameServer()->Server(); }

bool CAccounts::WaitForResult(const std::shared_ptr<ISqlResult> &pRes, const char *pOpName, int TimeoutMs)
{
	using namespace std::chrono;
	const auto Start = steady_clock::now();
	while(!pRes->m_Completed.load())
	{
		std::this_thread::sleep_for(1ms);
		if(duration_cast<milliseconds>(steady_clock::now() - Start).count() > TimeoutMs)
		{
			dbg_msg("accounts", "%s timed out waiting for DB result", pOpName);
			return false;
		}
	}
	return pRes->m_Success;
}

SHA256_DIGEST CAccounts::HashPassword(const char *pPassword)
{
	SHA256_CTX Sha256Ctx;
	sha256_init(&Sha256Ctx);
	sha256_update(&Sha256Ctx, pPassword, str_length(pPassword));
	return sha256_finish(&Sha256Ctx);
}

void CAccounts::Init(CGameContext *pGameServer, CDbConnectionPool *pPool)
{
	m_pGameServer = pGameServer;
	m_pPool = pPool;

	// Optional: mark all leftover sessions for this port as logged out
	LogoutAllAccountsPort(Server()->Port());
}

void CAccounts::AutoLogin(int ClientId)
{
	if(!m_pPool)
		return;

	const char *pName = Server()->ClientName(ClientId);

	// Query last user by LastPlayerName
	auto pRes = std::make_shared<CAccResult>();
	auto pReq = std::make_unique<CAccSelectByLastName>(pRes);
	str_copy(pReq->m_LastPlayerName, pName, sizeof(pReq->m_LastPlayerName));
	m_pPool->Execute(CAccountsWorker::SelectByLastPlayerName, std::move(pReq), "acc select by last name");

	if(!WaitForResult(pRes, "autologin.select"))
		return;
	if(!pRes->m_Found || pRes->m_LastLogin == 0)
		return;

	const char *pAddr = Server()->ClientAddrString(ClientId, false);
	if(str_comp(pRes->m_LastIP, pAddr) != 0)
		return;
	if(str_comp(pRes->m_Username, pName) != 0)
		return;
	if(pRes->m_LoggedIn || !(pRes->m_Flags & ACC_FLAG_AUTOLOGIN))
		return;

	if(ForceLogin(ClientId, pRes->m_Username))
		GameServer()->SendChatTarget(ClientId, "Automatically logged into your account");
	else
		GameServer()->SendChatTarget(ClientId, "[Err] Autologin Failed");
}

bool CAccounts::ForceLogin(int ClientId, const char *pUsername)
{
	if(!m_pPool || !pUsername[0])
		return false;

	// Query by Username
	auto pRes = std::make_shared<CAccResult>();
	auto pReq = std::make_unique<CAccSelectByUser>(pRes);
	str_copy(pReq->m_Username, pUsername, sizeof(pReq->m_Username));
	m_pPool->Execute(CAccountsWorker::SelectByUsername, std::move(pReq), "acc select by username");
	if(!WaitForResult(pRes, "forcelogin.select"))
		return false;

	if(!pRes->m_Found)
		return false;
	if(pRes->m_LoggedIn)
	{
		GameServer()->SendChatTarget(ClientId, "Account is already logged in");
		return false;
	}

	OnLogin(ClientId, *pRes);
	return true;
}

bool CAccounts::Login(int ClientId, const char *pUsername, const char *pPassword)
{
	if(!m_pPool)
		return false;

	if(!pUsername[0] || !pPassword[0])
		return false;

	char HashedPassword[ACC_MAX_PASSW_LENGTH];
	sha256_str(HashPassword(pPassword), HashedPassword, ACC_MAX_PASSW_LENGTH);

	// Use accounts_worker Login (SELECT with Username+Password)
	auto pRes = std::make_shared<CAccResult>();
	auto pReq = std::make_unique<CAccLoginRequest>(pRes);
	str_copy(pReq->m_Username, pUsername, sizeof(pReq->m_Username));
	str_copy(pReq->m_PasswordHash, HashedPassword, sizeof(pReq->m_PasswordHash));
	m_pPool->Execute(CAccountsWorker::Login, std::move(pReq), "acc login");

	if(!WaitForResult(pRes, "login.select"))
		return false;

	if(!pRes->m_Found)
		return false;
	if(pRes->m_LoggedIn)
	{
		GameServer()->SendChatTarget(ClientId, "Account is already logged in");
		return false;
	}

	OnLogin(ClientId, *pRes);
	return true;
}

void CAccounts::OnLogin(int ClientId, const CAccResult &Res)
{
	// Set account session (in-memory)
	{
		CAccountSession &Acc = GameServer()->m_Account[ClientId];

		time_t Now;
		time(&Now);

		str_copy(Acc.m_Username, Res.m_Username);
		Acc.m_RegisterDate = Res.m_RegisterDate;
		str_copy(Acc.m_Name, Res.m_PlayerName);
		str_copy(Acc.m_LastName, Res.m_LastPlayerName);
		str_copy(Acc.CurrentIp, Server()->ClientAddrString(ClientId, false));
		str_copy(Acc.LastIp, Res.m_LastIP);
		Acc.m_LoggedIn = true;
		Acc.m_LastLogin = Now;
		Acc.m_Port = Server()->Port();
		Acc.ClientId = ClientId;
		if(Res.m_Flags != -1)
			Acc.m_Flags = Res.m_Flags;
		if(Res.m_VoteMenuPage != -1)
			Acc.m_VoteMenuPage = Res.m_VoteMenuPage;
		Acc.m_Playtime = Res.m_Playtime;
		Acc.m_Deaths = Res.m_Deaths;
		Acc.m_Kills = Res.m_Kills;
		Acc.m_Level = Res.m_Level;
		Acc.m_XP = Res.m_XP;
		Acc.m_Money = Res.m_Money;
		str_copy(Acc.m_Inventory, Res.m_Inventory);
		str_copy(Acc.m_LastActiveItems, Res.m_LastActiveItems);

		Acc.m_LoginTick = Server()->Tick();

		GameServer()->m_VoteMenu.SetPage(ClientId, Acc.m_VoteMenuPage);
	}

	GameServer()->OnLogin(ClientId);

	// Persist login state
	auto pUpd = std::make_unique<CAccUpdLoginState>();
	str_copy(pUpd->m_Username, Res.m_Username, sizeof(pUpd->m_Username));
	str_copy(pUpd->m_PlayerName, Server()->ClientName(ClientId), sizeof(pUpd->m_PlayerName));
	str_copy(pUpd->m_CurrentIP, Server()->ClientAddrString(ClientId, false), sizeof(pUpd->m_CurrentIP));
	time_t Now;
	time(&Now);
	pUpd->m_LastLogin = Now;
	pUpd->m_Port = Server()->Port();
	pUpd->m_ClientId = ClientId;
	m_pPool->ExecuteWrite(CAccountsWorker::UpdateLoginState, std::move(pUpd), "acc update login");
}

bool CAccounts::Logout(int ClientId)
{
	if(GameServer()->m_Account[ClientId].m_LoggedIn)
	{
		OnLogout(ClientId, GameServer()->m_Account[ClientId]);
		GameServer()->OnLogout(ClientId);
		GameServer()->m_Account[ClientId] = CAccountSession(); // Reset account session
		return true;
	}
	return false;
}

void CAccounts::OnLogout(int ClientId, const CAccountSession AccInfo)
{
	if(!m_pPool)
		return;

	auto pReq = std::make_unique<CAccUpdLogoutState>();
	str_copy(pReq->m_Username, AccInfo.m_Username, sizeof(pReq->m_Username));
	pReq->m_Flags = AccInfo.m_Flags;
	pReq->m_VoteMenuPage = AccInfo.m_VoteMenuPage;
	pReq->m_Playtime = AccInfo.m_Playtime;
	pReq->m_Deaths = AccInfo.m_Deaths;
	pReq->m_Kills = AccInfo.m_Kills;
	pReq->m_Level = AccInfo.m_Level;
	pReq->m_XP = AccInfo.m_XP;
	pReq->m_Money = AccInfo.m_Money;
	str_copy(pReq->m_Inventory, AccInfo.m_Inventory);
	str_copy(pReq->m_LastActiveItems, AccInfo.m_LastActiveItems);

	m_pPool->ExecuteWrite(CAccountsWorker::UpdateLogoutState, std::move(pReq), "acc update logout");
}

void CAccounts::LogoutAllAccountsPort(int Port)
{
	if(!m_pPool)
		return;
	// Simple bulk logout by port
	struct CSqlLogoutByPort : ISqlData
	{
		CSqlLogoutByPort() :
			ISqlData(nullptr) {}
		int m_Port;
	};
	auto Fn = [](IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize) -> bool {
		const auto *p = dynamic_cast<const CSqlLogoutByPort *>(pData);
		char aSql[256];
		str_copy(aSql,
			"UPDATE foxnet_accounts "
			"SET LastPlayerName = PlayerName, LastIP = CurrentIP, LoggedIn = 0, Port = 0, ClientId = -1 "
			"WHERE Port = ?",
			sizeof(aSql));
		if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
			return false;
		pSql->BindInt(1, p->m_Port);
		int NumUpdated = 0;
		return pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
	};
	auto pReq = std::make_unique<CSqlLogoutByPort>();
	pReq->m_Port = Port;
	m_pPool->ExecuteWrite(Fn, std::move(pReq), "acc bulk logout by port");
}

bool CAccounts::Register(int ClientId, const char *pUsername, const char *pPassword, const char *pPassword2)
{
	if(!m_pPool)
		return false;

	if(!pUsername[0] || !pPassword[0] || !pPassword2[0])
	{
		GameServer()->SendChatTarget(ClientId, "Username or password is empty");
		return false;
	}
	if(str_comp(pPassword, pPassword2) != 0)
	{
		GameServer()->SendChatTarget(ClientId, "[Err] Passwords do not match");
		return false;
	}
	if(str_length(pUsername) > ACC_MAX_USERNAME_LENGTH - 1)
	{
		GameServer()->SendChatTarget(ClientId, "[Err] Username is too long");
		return false;
	}
	if(str_length(pUsername) < ACC_MIN_USERNAME_LENGTH)
	{
		GameServer()->SendChatTarget(ClientId, "[Err] Username is too short");
		return false;
	}
	if(str_length(pPassword) < ACC_MIN_PASSW_LENGTH)
	{
		GameServer()->SendChatTarget(ClientId, "[Err] Password is too short");
		return false;
	}

	char HashedPassword[ACC_MAX_PASSW_LENGTH];
	sha256_str(HashPassword(pPassword), HashedPassword, ACC_MAX_PASSW_LENGTH);

	auto pRes = std::make_shared<CAccResult>();
	auto pReq = std::make_unique<CAccRegisterRequest>(pRes);
	str_copy(pReq->m_Username, pUsername, sizeof(pReq->m_Username));
	str_copy(pReq->m_PasswordHash, HashedPassword, sizeof(pReq->m_PasswordHash));
	time_t Now;
	time(&Now);
	pReq->m_RegisterDate = Now;
	m_pPool->ExecuteWrite(CAccountsWorker::Register, std::move(pReq), "acc register");

	if(!WaitForResult(pRes, "register.insert"))
	{
		GameServer()->SendChatTarget(ClientId, "[Err] Registration failed");
		return false;
	}

	if(!pRes->m_Success)
	{
		GameServer()->SendChatTarget(ClientId, "[Err] Username is already taken");
		return false;
	}

	GameServer()->SendChatTarget(ClientId, "Registered Successfully, use /login..");
	return true;
}

bool CAccounts::ChangePassword(int ClientId, const char *pOldPassword, const char *pNewPassword, const char *pNewPassword2)
{
	if(!m_pPool)
		return false;

	if(!GameServer()->m_Account[ClientId].m_LoggedIn)
	{
		GameServer()->SendChatTarget(ClientId, "[Err] You are not logged in");
		return false;
	}

	if(!pOldPassword[0] || !pNewPassword[0] || !pNewPassword2[0])
	{
		GameServer()->SendChatTarget(ClientId, "[Err] Password is empty");
		return false;
	}
	if(str_comp(pNewPassword, pNewPassword2) != 0)
	{
		GameServer()->SendChatTarget(ClientId, "[Err] New passwords do not match");
		return false;
	}
	if(str_length(pNewPassword) < ACC_MIN_PASSW_LENGTH)
	{
		GameServer()->SendChatTarget(ClientId, "[Err] New password is too short");
		return false;
	}

	char HashedOld[ACC_MAX_PASSW_LENGTH];
	sha256_str(HashPassword(pOldPassword), HashedOld, ACC_MAX_PASSW_LENGTH);
	char HashedNew[ACC_MAX_PASSW_LENGTH];
	sha256_str(HashPassword(pNewPassword), HashedNew, ACC_MAX_PASSW_LENGTH);

	struct CSqlChangePass : ISqlData
	{
		CSqlChangePass() :
			ISqlData(nullptr) {}
		char m_Username[ACC_MAX_USERNAME_LENGTH];
		char m_OldHash[ACC_MAX_PASSW_LENGTH];
		char m_NewHash[ACC_MAX_PASSW_LENGTH];
	};
	auto Fn = [](IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize) -> bool {
		const auto *p = dynamic_cast<const CSqlChangePass *>(pData);
		const char *aSql =
			"UPDATE foxnet_accounts SET Password = ? WHERE Username = ? AND Password = ?";
		if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
			return false;
		pSql->BindString(1, p->m_NewHash);
		pSql->BindString(2, p->m_Username);
		pSql->BindString(3, p->m_OldHash);
		int NumUpdated = 0;
		return pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
	};
	auto pReq = std::make_unique<CSqlChangePass>();
	str_copy(pReq->m_Username, GameServer()->m_Account[ClientId].m_Username, sizeof(pReq->m_Username));
	str_copy(pReq->m_OldHash, HashedOld, sizeof(pReq->m_OldHash));
	str_copy(pReq->m_NewHash, HashedNew, sizeof(pReq->m_NewHash));

	m_pPool->ExecuteWrite(Fn, std::move(pReq), "acc change password");
	GameServer()->SendChatTarget(ClientId, "Password change requested");
	return true;
}

int CAccounts::IsAccountLoggedIn(const char *pUsername)
{
	if(!m_pPool || !pUsername[0])
		return 0;

	auto pRes = std::make_shared<CAccResult>();
	auto pReq = std::make_unique<CAccSelectPortByUser>(pRes);
	str_copy(pReq->m_Username, pUsername, sizeof(pReq->m_Username));
	m_pPool->Execute(CAccountsWorker::SelectPortByUsername, std::move(pReq), "acc select port");
	if(!WaitForResult(pRes, "isloggedin.select"))
		return 0;
	return pRes->m_Port;
}

void CAccounts::EditAccount(const char *pUsername, const char *pVariable, const char *pValue)
{
	if(!m_pPool)
		return;
	if(!pUsername || !pUsername[0] || !pVariable || !pVariable[0] || !pValue)
		return;

	const int LoggedPort = IsAccountLoggedIn(pUsername);
	if(LoggedPort != 0)
	{
		log_info("accounts", "Cannot edit account %s while user is logged in on port %d", pUsername, LoggedPort);
		return;
	}

	auto IsIntCol = [](const char *pCol) {
		static const char *s_aIntCols[] = {
			"Version", "RegisterDate", "LoggedIn", "LastLogin", "Port", "ClientId",
			"Flags", "VoteMenuPage", "Playtime", "Deaths", "Kills", "Level", "XP", "Money", nullptr};
		for(const char **pp = s_aIntCols; *pp; ++pp)
			if(!str_comp(*pp, pCol))
				return true;
		return false;
	};
	auto IsTextCol = [](const char *pCol) {
		static const char *s_aTextCols[] = {
			"PlayerName", "LastPlayerName", "CurrentIP", "LastIP", "Inventory", "LastActiveItems", nullptr};
		for(const char **pp = s_aTextCols; *pp; ++pp)
			if(!str_comp(*pp, pCol))
				return true;
		return false;
	};

	const bool IsInt = IsIntCol(pVariable);
	const bool IsText = IsTextCol(pVariable);

	if(!(IsInt || IsText))
	{
		log_info("accounts", "EditAccount: column '%s' is not editable", pVariable);
		return;
	}

	struct CSqlEditReq : ISqlData
	{
		CSqlEditReq() :
			ISqlData(nullptr) {}
		char m_Username[ACC_MAX_USERNAME_LENGTH];
		char m_Value[1028];
		char m_Column[64];
		bool m_IsInt;
	};
	auto Fn = [](IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize) -> bool {
		const auto *p = dynamic_cast<const CSqlEditReq *>(pData);
		char aSql[384];
		if(p->m_IsInt)
			str_format(aSql, sizeof(aSql), "UPDATE foxnet_accounts SET %s = CAST(? AS INTEGER) WHERE Username = ?", p->m_Column);
		else
			str_format(aSql, sizeof(aSql), "UPDATE foxnet_accounts SET %s = ? WHERE Username = ?", p->m_Column);
		if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
			return false;
		pSql->BindString(1, p->m_Value);
		pSql->BindString(2, p->m_Username);
		int NumUpdated = 0;
		return pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
	};
	auto pReq = std::make_unique<CSqlEditReq>();
	str_copy(pReq->m_Username, pUsername, sizeof(pReq->m_Username));
	str_copy(pReq->m_Value, pValue, sizeof(pReq->m_Value));
	str_copy(pReq->m_Column, pVariable, sizeof(pReq->m_Column));
	pReq->m_IsInt = IsInt;

	m_pPool->ExecuteWrite(Fn, std::move(pReq), "acc edit");
}

static const char *FormatLastSeen(time_t LastLoginDate)
{
	static char aBuf[64];
	if(LastLoginDate <= 0)
	{
		str_copy(aBuf, "Never", sizeof(aBuf));
		return aBuf;
	}

	time_t Now;
	time(&Now);

	double Seconds = difftime(Now, LastLoginDate);
	int Days = (int)(Seconds / (60 * 60 * 24));
	int Hours = (int)(Seconds / (60 * 60));

	if(Days > 0)
		str_format(aBuf, sizeof(aBuf), "Last seen %d day%s ago", Days, Days == 1 ? "" : "s");
	else if(Hours > 0)
		str_format(aBuf, sizeof(aBuf), "Last seen %d hour%s ago", Hours, Hours == 1 ? "" : "s");
	else
		str_copy(aBuf, "Last seen less than an hour ago", sizeof(aBuf));

	return aBuf;
}

void CAccounts::ShowAccProfile(int ClientId, const char *pName)
{
	auto OptAcc = GetAccountCurName(pName);

	if(!OptAcc)
		OptAcc = GetAccount(pName);

	char aBuf[128];
	if(!OptAcc)
	{
		GameServer()->SendChatTarget(ClientId, "╭─────────       Pʀᴏғɪʟᴇ");
		str_format(aBuf, sizeof(aBuf), "│ Account \"%s\" doesn't exist", pName);
		GameServer()->SendChatTarget(ClientId, aBuf);

		GameServer()->SendChatTarget(ClientId, "╰──────────────────────────");
		return;
	}

	GameServer()->SendChatTarget(ClientId, "╭──────     Pʀᴏғɪʟᴇ");

	str_format(aBuf, sizeof(aBuf), "│ Name: %s", OptAcc->m_LoggedIn ? pName : OptAcc->m_Name);
	GameServer()->SendChatTarget(ClientId, aBuf);

	str_format(aBuf, sizeof(aBuf), "│ Status: %s", OptAcc->m_LoggedIn ? "Online" : "Offline");
	GameServer()->SendChatTarget(ClientId, aBuf);

	if(!OptAcc->m_LoggedIn)
	{
		str_format(aBuf, sizeof(aBuf), "│ %s", FormatLastSeen(OptAcc->m_LastLogin));
		GameServer()->SendChatTarget(ClientId, aBuf);
	}

	GameServer()->SendChatTarget(ClientId, "├──────      Sᴛᴀᴛs");

	str_format(aBuf, sizeof(aBuf), "│ Level %lld", OptAcc->m_Level);
	GameServer()->SendChatTarget(ClientId, aBuf);
	str_format(aBuf, sizeof(aBuf), "│ %lld %s", OptAcc->m_Money, g_Config.m_SvCurrencyName);
	GameServer()->SendChatTarget(ClientId, aBuf);

	float PlayTimeHours = OptAcc->m_Playtime / 60.0f;
	str_format(aBuf, sizeof(aBuf), "│ %.1f Hours Playtime", PlayTimeHours);
	if(OptAcc->m_Playtime < 100)
		str_format(aBuf, sizeof(aBuf), "│ %d Minutes Playtime", (int)OptAcc->m_Playtime);
	GameServer()->SendChatTarget(ClientId, aBuf);

	str_format(aBuf, sizeof(aBuf), "│ %lld Deaths", OptAcc->m_Deaths);
	GameServer()->SendChatTarget(ClientId, aBuf);

	GameServer()->SendChatTarget(ClientId, "╰───────────────────────");
}

std::optional<CAccountSession> CAccounts::GetAccount(const char *pUsername)
{
	if(!m_pPool || !pUsername[0])
		return std::nullopt;

	auto pRes = std::make_shared<CAccResult>();
	auto pReq = std::make_unique<CAccSelectByUser>(pRes);
	str_copy(pReq->m_Username, pUsername, sizeof(pReq->m_Username));
	m_pPool->Execute(CAccountsWorker::SelectByUsername, std::move(pReq), "acc select by username (profile)");
	if(!WaitForResult(pRes, "getaccount.select"))
		return std::nullopt;

	if(!pRes->m_Found)
		return std::nullopt;

	CAccountSession Acc;
	str_copy(Acc.m_Username, pRes->m_Username);
	str_copy(Acc.m_Name, pRes->m_PlayerName);
	str_copy(Acc.m_LastName, pRes->m_LastPlayerName);
	Acc.m_LoggedIn = pRes->m_LoggedIn;
	Acc.m_LastLogin = pRes->m_LastLogin;
	Acc.m_Playtime = pRes->m_Playtime;
	Acc.m_Deaths = pRes->m_Deaths;
	Acc.m_Level = pRes->m_Level;
	Acc.m_Money = pRes->m_Money;
	return Acc;
}

std::optional<CAccountSession> CAccounts::GetAccountCurName(const char *pName)
{
	if(!m_pPool || !pName[0])
		return std::nullopt;

	auto pRes = std::make_shared<CAccResult>();
	auto pReq = std::make_unique<CAccSelectByLastName>(pRes);
	str_copy(pReq->m_LastPlayerName, pName, sizeof(pReq->m_LastPlayerName));
	m_pPool->Execute(CAccountsWorker::SelectByLastPlayerName, std::move(pReq), "acc select by last name (profile)");
	if(!WaitForResult(pRes, "getaccountcurname.select"))
		return std::nullopt;

	if(!pRes->m_Found)
		return std::nullopt;

	CAccountSession Acc;
	str_copy(Acc.m_Username, pRes->m_Username);
	str_copy(Acc.m_Name, pRes->m_PlayerName);
	str_copy(Acc.m_LastName, pRes->m_LastPlayerName);
	Acc.m_LoggedIn = pRes->m_LoggedIn;
	Acc.m_LastLogin = pRes->m_LastLogin;
	Acc.m_Playtime = pRes->m_Playtime;
	Acc.m_Deaths = pRes->m_Deaths;
	Acc.m_Level = pRes->m_Level;
	Acc.m_Money = pRes->m_Money;
	return Acc;
}

CAccountSession CAccounts::GetAccount(int ClientId)
{
	return GameServer()->m_Account[ClientId];
}

void CAccounts::SaveAccountsInfo(int ClientId, const CAccountSession AccInfo)
{
	if(!m_pPool)
		return;

	auto pReq = std::make_unique<CAccSaveInfo>();
	pReq->m_Flags = AccInfo.m_Flags;
	pReq->m_VoteMenuPage = AccInfo.m_VoteMenuPage;
	pReq->m_Playtime = AccInfo.m_Playtime;
	pReq->m_Deaths = AccInfo.m_Deaths;
	pReq->m_Kills = AccInfo.m_Kills;
	pReq->m_Level = AccInfo.m_Level;
	pReq->m_XP = AccInfo.m_XP;
	pReq->m_Money = AccInfo.m_Money;
	str_copy(pReq->m_Inventory, AccInfo.m_Inventory);
	str_copy(pReq->m_LastActiveItems, AccInfo.m_LastActiveItems);
	str_copy(pReq->m_Username, AccInfo.m_Username);
	m_pPool->ExecuteWrite(CAccountsWorker::SaveInfo, std::move(pReq), "acc save info");
}

void CAccounts::SaveAllAccounts()
{
	if(!m_pPool)
		return;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_Account[i].m_LoggedIn)
			SaveAccountsInfo(i, GameServer()->m_Account[i]);
	}
}

void CAccounts::Top5(int ClientId, const char *pType, int Offset)
{
	if(!m_pPool)
		return;

	auto pReq = std::make_unique<CAccShowTop5>();
	pReq->m_ClientId = ClientId;
	str_copy(pReq->m_Type, pType, sizeof(pReq->m_Type));
	pReq->m_Offset = Offset;
	pReq->m_pGameServer = GameServer();

	m_pPool->Execute(CAccountsWorker::ShowTop5, std::move(pReq), "acc top5");
}

void CAccounts::SetPlayerName(int ClientId, const char *pName) // When player changes name
{
	if(!m_pPool)
		return;

	auto pReq = std::make_unique<CAccSetNameReq>();
	str_copy(pReq->m_NewPlayerName, pName, sizeof(pReq->m_NewPlayerName));
	str_copy(pReq->m_Username, GameServer()->m_Account[ClientId].m_Username, sizeof(pReq->m_Username));
	m_pPool->ExecuteWrite(CAccountsWorker::SetPlayerName, std::move(pReq), "acc set player name");
}

int CAccounts::NeededXP(int Level)
{
	if(Level < 1)
		return 30;
	else if(Level < 5)
		return 65;
	else if(Level < 10)
		return 100;
	else if(Level < 20)
		return 150;
	else
		return 150 + Level * 2;
}