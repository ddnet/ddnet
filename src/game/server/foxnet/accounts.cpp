#include "accounts.h"
#include "../gamecontext.h"
#include <base/hash.h>
#include <base/hash_ctxt.h>
#include <base/log.h>
#include <ctime>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <sqlite3.h>

IServer *CAccounts::Server() const { return GameServer()->Server(); }

constexpr const char *SQLITE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS Accounts (
			Version INTEGER NOT NULL DEFAULT 1,
            Username TEXT NOT NULL COLLATE NOCASE,
            Password TEXT NOT NULL,
            RegisterDate INTEGER NOT NULL,
			PlayerName TEXT DEFAULT '',
            LastPlayerName TEXT DEFAULT '',
			CurrentIP TEXT DEFAULT '',
			LastIP TEXT DEFAULT '',
            LoggedIn INTEGER DEFAULT 0,
			LastLogin INTEGER DEFAULT 0,
            Port INTEGER DEFAULT 0,
            ClientId INTEGER DEFAULT -1,
            Flags INTEGER DEFAULT 0,
            VoteMenuPage INTEGER DEFAULT 0,
			Playtime INTEGER DEFAULT 0,
			Deaths INTEGER DEFAULT 0,
			Kills INTEGER DEFAULT 0,
            Level INTEGER DEFAULT 0,
            XP INTEGER DEFAULT 0,
			Money INTEGER DEFAULT 0,
			Inventory TEXT DEFAULT '',
			LastActiveItems TEXT DEFAULT '',
			PRIMARY KEY (Username)
        );
    )";

SHA256_DIGEST CAccounts::HashPassword(const char *pPassword)
{
	SHA256_CTX Sha256Ctx;
	sha256_init(&Sha256Ctx);
	sha256_update(&Sha256Ctx, pPassword, str_length(pPassword));
	return sha256_finish(&Sha256Ctx);
}

void CAccounts::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	if(sqlite3_open("Accounts.sqlite", &m_AccDatabase) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to open SQLite database: %s", sqlite3_errmsg(m_AccDatabase));
		return;
	}

	LogoutAllAccountsPort(Server()->Port());
}

void CAccounts::OnShutdown()
{
	if(m_AccDatabase)
		sqlite3_close(m_AccDatabase);
}

void CAccounts::AutoLogin(int ClientId)
{
	if(!m_AccDatabase)
		return;

	const char *SelectQuery = R"(
        SELECT Username, LastIP, LastLogin, LoggedIn, Flags
        FROM Accounts
        WHERE LastPlayerName = ?;
    )";
	sqlite3_stmt *stmt;
	if(sqlite3_prepare_v2(m_AccDatabase, SelectQuery, -1, &stmt, nullptr) != SQLITE_OK)
		return;

	const char *pName = Server()->ClientName(ClientId);
	sqlite3_bind_text(stmt, 1, pName, -1, SQLITE_STATIC);

	CAccountSession Acc = CAccountSession();
	if(sqlite3_step(stmt) == SQLITE_ROW)
	{
		str_copy(Acc.m_Username, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
		str_copy(Acc.LastIp, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
		Acc.m_LastLogin = sqlite3_column_int64(stmt, 2);
		Acc.m_LoggedIn = sqlite3_column_int(stmt, 3);
		Acc.m_Flags = sqlite3_column_int64(stmt, 4);
	}
	sqlite3_finalize(stmt);

	if(Acc.m_LastLogin == 0)
		return;
	const char *pAddr = Server()->ClientAddrString(ClientId, false);
	if(str_comp(Acc.LastIp, pAddr) != 0)
		return;
	if(str_comp(Acc.m_Username, pName) != 0)
		return;
	if(Acc.m_LoggedIn || !(Acc.m_Flags & ACC_FLAG_AUTOLOGIN))
		return;

	if(ForceLogin(ClientId, Acc.m_Username))
		GameServer()->SendChatTarget(ClientId, "Automatically logged into your account");
	else
		GameServer()->SendChatTarget(ClientId, "[Err] Autologin Failed");
}

bool CAccounts::ForceLogin(int ClientId, const char *pUsername)
{
	if(!m_AccDatabase)
		return false;

	if(!pUsername[0])
		return false;

	const char *SelectQuery = R"(
		SELECT Username, RegisterDate, PlayerName, LastPlayerName, CurrentIP, LastIP, LoggedIn, LastLogin, Port, ClientId, Flags, VoteMenuPage, Playtime, Deaths, Kills, Level, XP, Money, Inventory, LastActiveItems
        FROM Accounts
        WHERE Username = ?;
    )";

	sqlite3_stmt *stmt;
	if(sqlite3_prepare_v2(m_AccDatabase, SelectQuery, -1, &stmt, nullptr) != SQLITE_OK)
		return false;

	sqlite3_bind_text(stmt, 1, pUsername, -1, SQLITE_STATIC);

	if(sqlite3_step(stmt) == SQLITE_ROW)
	{
		if(sqlite3_column_int(stmt, 6))
		{
			sqlite3_finalize(stmt);
			GameServer()->SendChatTarget(ClientId, "Account is already logged in");
			return false;
		}

		OnLogin(ClientId, pUsername, stmt);
		sqlite3_finalize(stmt);
		return true;
	}
	sqlite3_finalize(stmt);

	return false;
}

bool CAccounts::Login(int ClientId, const char *pUsername, const char *pPassword)
{
	if(!m_AccDatabase)
		return false;

	if(!pUsername[0] || !pPassword[0])
		return false;

	const char *SelectQuery = R"(
		SELECT Username, RegisterDate, PlayerName, LastPlayerName, CurrentIP, LastIP, LoggedIn, LastLogin, Port, ClientId, Flags, VoteMenuPage, Playtime, Deaths, Kills, Level, XP, Money, Inventory, LastActiveItems
        FROM Accounts
        WHERE Username = ? AND Password = ?;
    )";
	sqlite3_stmt *stmt;

	if(sqlite3_prepare_v2(m_AccDatabase, SelectQuery, -1, &stmt, nullptr) != SQLITE_OK)
		return false;

	char HashedPassword[ACC_MAX_PASSW_LENGTH];
	sha256_str(HashPassword(pPassword), HashedPassword, ACC_MAX_PASSW_LENGTH);

	sqlite3_bind_text(stmt, 1, pUsername, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, HashedPassword, -1, SQLITE_STATIC);

	if(sqlite3_step(stmt) == SQLITE_ROW)
	{
		if(sqlite3_column_int(stmt, 6))
		{
			sqlite3_finalize(stmt);
			GameServer()->SendChatTarget(ClientId, "Account is already logged in");
			return false;
		}
		OnLogin(ClientId, pUsername, stmt);
		sqlite3_finalize(stmt);
		return true;
	}
	sqlite3_finalize(stmt);

	return false;
}

void CAccounts::OnLogin(int ClientId, const char *pUsername, sqlite3_stmt *pstmt)
{
	if(!m_AccDatabase)
		return;

	if(sqlite3_exec(m_AccDatabase, SQLITE_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to create table: %s", sqlite3_errmsg(m_AccDatabase));
		sqlite3_close(m_AccDatabase);
		return;
	}

	// Set account session
	{
		CAccountSession &Acc = GameServer()->m_Account[ClientId];

		time_t Now;
		time(&Now);

		str_copy(Acc.m_Username, reinterpret_cast<const char *>(sqlite3_column_text(pstmt, 0)));
		Acc.m_RegisterDate = sqlite3_column_int64(pstmt, 1);
		str_copy(Acc.m_Name, reinterpret_cast<const char *>(sqlite3_column_text(pstmt, 2)));
		str_copy(Acc.m_LastName, reinterpret_cast<const char *>(sqlite3_column_text(pstmt, 3)));
		str_copy(Acc.CurrentIp, Server()->ClientAddrString(ClientId, false));
		str_copy(Acc.LastIp, reinterpret_cast<const char *>(sqlite3_column_text(pstmt, 5)));
		Acc.m_LoggedIn = true;
		Acc.m_LastLogin = Now;
		Acc.m_Port = Server()->Port();
		Acc.ClientId = ClientId;
		Acc.m_Flags = sqlite3_column_int64(pstmt, 10);
		Acc.m_VoteMenuPage = sqlite3_column_int64(pstmt, 11);
		Acc.m_Playtime = sqlite3_column_int64(pstmt, 12);
		Acc.m_Deaths = sqlite3_column_int64(pstmt, 13);
		Acc.m_Kills = sqlite3_column_int64(pstmt, 14);
		Acc.m_Level = sqlite3_column_int64(pstmt, 15);
		Acc.m_XP = sqlite3_column_int64(pstmt, 16);
		Acc.m_Money = sqlite3_column_int64(pstmt, 17);
		str_copy(Acc.m_Inventory, reinterpret_cast<const char *>(sqlite3_column_text(pstmt, 18)));
		str_copy(Acc.m_LastActiveItems, reinterpret_cast<const char *>(sqlite3_column_text(pstmt, 19)));

		Acc.m_LoginTick = Server()->Tick();

		GameServer()->m_VoteMenu.SetPage(ClientId, Acc.m_VoteMenuPage);
	}

	const char *CurrentIP = Server()->ClientAddrString(ClientId, false);
	int Port = Server()->Port();
	const char *PlayerName = Server()->ClientName(ClientId);

	const char *UpdateQuery = R"(
		UPDATE Accounts
		SET
			LastPlayerName = PlayerName,
			PlayerName = ?,
			LastIP = CurrentIP,
			CurrentIP = ?,
			LoggedIn = 1,
			LastLogin = ?,	
			Port = ?,
			ClientId = ?
		WHERE Username = ?;
	)";

	sqlite3_stmt *stmt = nullptr;
	if(sqlite3_prepare_v2(m_AccDatabase, UpdateQuery, -1, &stmt, nullptr) != SQLITE_OK)
		return;

	time_t Now;
	time(&Now);

	sqlite3_bind_text(stmt, 1, PlayerName, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, CurrentIP, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, Now);
	sqlite3_bind_int(stmt, 4, Port);
	sqlite3_bind_int(stmt, 5, ClientId);
	sqlite3_bind_text(stmt, 6, pUsername, -1, SQLITE_STATIC);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

bool CAccounts::Logout(int ClientId)
{
	if(GameServer()->m_Account[ClientId].m_LoggedIn)
	{
		OnLogout(ClientId, GameServer()->m_Account[ClientId]);
		GameServer()->m_Account[ClientId] = CAccountSession(); // Reset account session
		return true;
	}
	return false;
}

void CAccounts::OnLogout(int ClientId, const CAccountSession AccInfo)
{
	if(!m_AccDatabase)
		return;

	if(sqlite3_exec(m_AccDatabase, SQLITE_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to create table: %s", sqlite3_errmsg(m_AccDatabase));
		sqlite3_close(m_AccDatabase);
		return;
	}

	const char *UpdateQuery = R"(
		UPDATE Accounts
		SET
			LoggedIn = 0,
			Port = 0,
			ClientId = -1,
			LastPlayerName = PlayerName,
			LastIP = CurrentIP,
			Flags = ?,
			VoteMenuPage = ?,
			Playtime = ?,
			Deaths = ?,
			Kills = ?,
			Level = ?,
			XP = ?,
			Money = ?,
			Inventory = ?,
			LastActiveItems = ?
		WHERE Username = ?;
	)";

	sqlite3_stmt *stmt = nullptr;
	if(sqlite3_prepare_v2(m_AccDatabase, UpdateQuery, -1, &stmt, nullptr) == SQLITE_OK)
	{
		time_t Now;
		time(&Now);

		sqlite3_bind_int64(stmt, 1, AccInfo.m_Flags);
		sqlite3_bind_int64(stmt, 2, AccInfo.m_VoteMenuPage);
		sqlite3_bind_int64(stmt, 3, AccInfo.m_Playtime);
		sqlite3_bind_int64(stmt, 4, AccInfo.m_Deaths);
		sqlite3_bind_int64(stmt, 5, AccInfo.m_Kills);
		sqlite3_bind_int64(stmt, 6, AccInfo.m_Level);
		sqlite3_bind_int64(stmt, 7, AccInfo.m_XP);
		sqlite3_bind_int64(stmt, 8, AccInfo.m_Money);
		sqlite3_bind_text(stmt, 9, AccInfo.m_Inventory, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 10, AccInfo.m_LastActiveItems, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 11, AccInfo.m_Username, -1, SQLITE_STATIC);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void CAccounts::LogoutAllAccountsPort(int Port)
{
	if(!m_AccDatabase)
		return;
	if(sqlite3_exec(m_AccDatabase, SQLITE_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to create table: %s", sqlite3_errmsg(m_AccDatabase));
		sqlite3_close(m_AccDatabase);
		return;
	}
	const char *UpdateQuery = R"(
		UPDATE Accounts
		SET
			LastPlayerName = PlayerName,
			LastIP = CurrentIP,
			LoggedIn = 0,
			Port = 0,
			ClientId = -1
		WHERE Port = ?;
	)";
	sqlite3_stmt *stmt = nullptr;
	if(sqlite3_prepare_v2(m_AccDatabase, UpdateQuery, -1, &stmt, nullptr) != SQLITE_OK)
		return;

	sqlite3_bind_int(stmt, 1, Port);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

bool CAccounts::Register(int ClientId, const char *pUsername, const char *pPassword, const char *pPassword2)
{
	if(!m_AccDatabase)
		return false;

	if(sqlite3_exec(m_AccDatabase, SQLITE_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to create table: %s", sqlite3_errmsg(m_AccDatabase));
		sqlite3_close(m_AccDatabase);
		return false;
	}

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

	// Use plain INSERT so duplicates trigger a constraint error (do not overwrite)
	const char *InsertQuery = R"(
        INSERT INTO Accounts (Username, Password, RegisterDate)
        VALUES (?, ?, ?);
    )";

	sqlite3_stmt *stmt = nullptr;
	if(sqlite3_prepare_v2(m_AccDatabase, InsertQuery, -1, &stmt, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to prepare insert statement: %s", sqlite3_errmsg(m_AccDatabase));
		return false;
	}

	time_t Now;
	time(&Now);

	sqlite3_bind_text(stmt, 1, pUsername, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, HashedPassword, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt, 3, Now);

	int rc = sqlite3_step(stmt);
	if(rc != SQLITE_DONE)
	{
		int err = sqlite3_errcode(m_AccDatabase);
		if(err == SQLITE_CONSTRAINT)
		{
			GameServer()->SendChatTarget(ClientId, "[Err] Username is already taken");
		}
		else
		{
			dbg_msg("accounts", "Failed to insert account info: (%d) %s", err, sqlite3_errmsg(m_AccDatabase));
		}
		sqlite3_finalize(stmt);
		return false;
	}

	GameServer()->SendChatTarget(ClientId, "Registered Successfully, use /login..");

	sqlite3_finalize(stmt);
	return true;
}

bool CAccounts::ChangePassword(int ClientId, const char *pOldPassword, const char *pNewPassword, const char *pNewPassword2)
{
	if(!m_AccDatabase)
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
	char HashedOldPassword[ACC_MAX_PASSW_LENGTH];
	sha256_str(HashPassword(pOldPassword), HashedOldPassword, ACC_MAX_PASSW_LENGTH);
	char HashedNewPassword[ACC_MAX_PASSW_LENGTH];
	sha256_str(HashPassword(pNewPassword), HashedNewPassword, ACC_MAX_PASSW_LENGTH);
	const char *UpdateQuery = R"(
		UPDATE Accounts
		SET Password = ?
		WHERE Username = ? AND Password = ?;
	)";
	sqlite3_stmt *stmt = nullptr;
	if(sqlite3_prepare_v2(m_AccDatabase, UpdateQuery, -1, &stmt, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to prepare update statement: %s", sqlite3_errmsg(m_AccDatabase));
		return false;
	}
	sqlite3_bind_text(stmt, 1, HashedNewPassword, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, GameServer()->m_Account[ClientId].m_Username, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, HashedOldPassword, -1, SQLITE_STATIC);

	int rc = sqlite3_step(stmt);
	if(rc != SQLITE_DONE)
	{
		int err = sqlite3_errcode(m_AccDatabase);
		if(err == SQLITE_CONSTRAINT)
		{
			GameServer()->SendChatTarget(ClientId, "[Err] Old password is incorrect");
		}
		else
		{
			GameServer()->SendChatTarget(ClientId, "[Err] Failed to change password");
		}
		sqlite3_finalize(stmt);
		return false;
	}

	return true;
}

int CAccounts::IsAccountLoggedIn(const char *pUsername)
{
	if(!m_AccDatabase)
		return false;
	if(!pUsername[0])
		return false;
	const char *SelectQuery = R"(
		SELECT Port
		FROM Accounts
		WHERE Username = ?;
	)";
	sqlite3_stmt *stmt;
	if(sqlite3_prepare_v2(m_AccDatabase, SelectQuery, -1, &stmt, nullptr) != SQLITE_OK)
		return false;
	sqlite3_bind_text(stmt, 1, pUsername, -1, SQLITE_STATIC);
	int LoggedIn = false;
	if(sqlite3_step(stmt) == SQLITE_ROW)
	{
		LoggedIn = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return LoggedIn;
}

void CAccounts::EditAccount(const char *pUsername, const char *pVariable, const char *pValue)
{
	if(!m_AccDatabase)
		return;
	if(sqlite3_exec(m_AccDatabase, SQLITE_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to create table: %s", sqlite3_errmsg(m_AccDatabase));
		sqlite3_close(m_AccDatabase);
		return;
	}

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
		{
			if(!str_comp(*pp, pCol))
				return true;
		}
		return false;
	};
	auto IsTextCol = [](const char *pCol) {
		static const char *s_aTextCols[] = {
			"PlayerName", "LastPlayerName", "CurrentIP", "LastIP", "Inventory", "LastActiveItems", nullptr};
		for(const char **pp = s_aTextCols; *pp; ++pp)
		{
			if(!str_comp(*pp, pCol))
				return true;
		}
		return false;
	};

	const bool IsInt = IsIntCol(pVariable);
	const bool IsText = IsTextCol(pVariable);

	if(!(IsInt || IsText))
	{
		log_info("accounts", "EditAccount: column '%s' is not editable", pVariable);
		return;
	}

	char aSql[256];
	if(IsInt)
		str_format(aSql, sizeof(aSql), "UPDATE Accounts SET %s = CAST(? AS INTEGER) WHERE Username = ?;", pVariable);
	else // text
		str_format(aSql, sizeof(aSql), "UPDATE Accounts SET %s = ? WHERE Username = ?;", pVariable);

	sqlite3_stmt *pStmt = nullptr;
	if(sqlite3_prepare_v2(m_AccDatabase, aSql, -1, &pStmt, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "EditAccount: prepare failed: %s", sqlite3_errmsg(m_AccDatabase));
		return;
	}

	sqlite3_bind_text(pStmt, 1, pValue, -1, SQLITE_STATIC);
	sqlite3_bind_text(pStmt, 2, pUsername, -1, SQLITE_STATIC);

	const int rc = sqlite3_step(pStmt);
	if(rc != SQLITE_DONE)
		dbg_msg("accounts", "update failed: (%d) %s", sqlite3_errcode(m_AccDatabase), sqlite3_errmsg(m_AccDatabase));
	sqlite3_finalize(pStmt);
}

const char *FormatLastSeen(time_t LastLoginDate)
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

	str_format(aBuf, sizeof(aBuf), "│ Level %d", OptAcc->m_Level);
	GameServer()->SendChatTarget(ClientId, aBuf);
	str_format(aBuf, sizeof(aBuf), "│ %lld %s", OptAcc->m_Money, g_Config.m_SvCurrencyName);
	GameServer()->SendChatTarget(ClientId, aBuf);

	float PlayTimeHours = OptAcc->m_Playtime / 60.0f;
	str_format(aBuf, sizeof(aBuf), "│ %.1f Hours Playtime", PlayTimeHours);
	if(OptAcc->m_Playtime < 100)
		str_format(aBuf, sizeof(aBuf), "│ %d Minutes Playtime", (int)OptAcc->m_Playtime);
	GameServer()->SendChatTarget(ClientId, aBuf);

	str_format(aBuf, sizeof(aBuf), "│ %d Deaths", OptAcc->m_Deaths);
	GameServer()->SendChatTarget(ClientId, aBuf);

	// if(g_Config.m_SvBlockMode)
	//{
	//	str_format(aBuf, sizeof(aBuf), "│ %d Kills", pAcc->m_Kills);
	//	GameServer()->SendChatTarget(ClientId, aBuf);
	// }

	GameServer()->SendChatTarget(ClientId, "╰───────────────────────");
}

std::optional<CAccountSession> CAccounts::GetAccount(const char *pUsername)
{
	if(!m_AccDatabase)
		return std::nullopt;
	if(!pUsername[0])
		return std::nullopt;
	const char *SelectQuery = R"(
		SELECT Username, PlayerName, LastPlayerName, LoggedIn, LastLogin, Playtime, Deaths, Level, Money
		FROM Accounts
		WHERE Username = ?;
	)";
	sqlite3_stmt *stmt;
	if(sqlite3_prepare_v2(m_AccDatabase, SelectQuery, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, pUsername, -1, SQLITE_STATIC);
		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
			CAccountSession Acc;
			str_copy(Acc.m_Username, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
			str_copy(Acc.m_Name, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
			str_copy(Acc.m_LastName, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
			Acc.m_LoggedIn = sqlite3_column_int(stmt, 3);
			Acc.m_LastLogin = sqlite3_column_int64(stmt, 4);
			Acc.m_Playtime = sqlite3_column_int64(stmt, 5);
			Acc.m_Deaths = sqlite3_column_int64(stmt, 6);
			Acc.m_Level = sqlite3_column_int64(stmt, 7);
			Acc.m_Money = sqlite3_column_int64(stmt, 8);
			sqlite3_finalize(stmt);
			return Acc;
		}
		sqlite3_finalize(stmt);
	}
	return std::nullopt;
}

std::optional<CAccountSession> CAccounts::GetAccountCurName(const char *pName)
{
	if(!m_AccDatabase)
		return std::nullopt;
	if(!pName[0])
		return std::nullopt;
	const char *SelectQuery = R"(
		SELECT Username, PlayerName, LastPlayerName, LoggedIn, LastLogin, Playtime, Deaths, Level, Money
		FROM Accounts
		WHERE LastPlayerName = ?;
	)";
	sqlite3_stmt *stmt;
	if(sqlite3_prepare_v2(m_AccDatabase, SelectQuery, -1, &stmt, nullptr) == SQLITE_OK)
	{
		sqlite3_bind_text(stmt, 1, pName, -1, SQLITE_STATIC);
		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
			CAccountSession Acc;
			str_copy(Acc.m_Username, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
			str_copy(Acc.m_Name, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
			str_copy(Acc.m_LastName, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
			Acc.m_LoggedIn = sqlite3_column_int(stmt, 3);
			Acc.m_LastLogin = sqlite3_column_int64(stmt, 4);
			Acc.m_Playtime = sqlite3_column_int64(stmt, 5);
			Acc.m_Deaths = sqlite3_column_int64(stmt, 6);
			Acc.m_Level = sqlite3_column_int64(stmt, 7);
			Acc.m_Money = sqlite3_column_int64(stmt, 8);
			sqlite3_finalize(stmt);
			return Acc;
		}
		sqlite3_finalize(stmt);
	}
	return std::nullopt;
}

CAccountSession CAccounts::GetAccount(int ClientId)
{
	return GameServer()->m_Account[ClientId];
}

void CAccounts::SaveAccountsInfo(int ClientId, const CAccountSession AccInfo)
{
	if(!m_AccDatabase)
		return;

	if(sqlite3_exec(m_AccDatabase, SQLITE_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to create table: %s", sqlite3_errmsg(m_AccDatabase));
		sqlite3_close(m_AccDatabase);
		return;
	}

	const char *UpdateQuery = R"(
		UPDATE Accounts
		SET
			LastPlayerName = PlayerName,
			LastIP = CurrentIP,
			Flags = ?,
			VoteMenuPage = ?,
			Playtime = ?,
			Deaths = ?,
			Kills = ?,
			Level = ?,
			XP = ?,
			Money = ?,
			Inventory = ?,
			LastActiveItems = ?
		WHERE Username = ?;
	)";

	sqlite3_stmt *stmt = nullptr;
	if(sqlite3_prepare_v2(m_AccDatabase, UpdateQuery, -1, &stmt, nullptr) == SQLITE_OK)
	{
		time_t Now;
		time(&Now);

		sqlite3_bind_int64(stmt, 1, AccInfo.m_Flags);
		sqlite3_bind_int64(stmt, 2, AccInfo.m_VoteMenuPage);
		sqlite3_bind_int64(stmt, 3, AccInfo.m_Playtime);
		sqlite3_bind_int64(stmt, 4, AccInfo.m_Deaths);
		sqlite3_bind_int64(stmt, 5, AccInfo.m_Kills);
		sqlite3_bind_int64(stmt, 6, AccInfo.m_Level);
		sqlite3_bind_int64(stmt, 7, AccInfo.m_XP);
		sqlite3_bind_int64(stmt, 8, AccInfo.m_Money);
		sqlite3_bind_text(stmt, 9, AccInfo.m_Inventory, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 10, AccInfo.m_LastActiveItems, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 11, AccInfo.m_Username, -1, SQLITE_STATIC);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void CAccounts::SaveAllAccounts()
{
	if(!m_AccDatabase)
		return;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_Account[i].m_LoggedIn)
			SaveAccountsInfo(i, GameServer()->m_Account[i]);
	}
}

void CAccounts::SetPlayerName(int ClientId, const char *pName) // When player changes name
{
	if(!m_AccDatabase)
		return;

	if(sqlite3_exec(m_AccDatabase, SQLITE_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK)
	{
		dbg_msg("accounts", "Failed to create table: %s", sqlite3_errmsg(m_AccDatabase));
		sqlite3_close(m_AccDatabase);
		return;
	}

	const char *UpdateQuery = R"(
		UPDATE Accounts
		SET
			LastPlayerName = PlayerName,
			PlayerName = ?
		WHERE Username = ?;
	)";

	sqlite3_stmt *stmt = nullptr;
	if(sqlite3_prepare_v2(m_AccDatabase, UpdateQuery, -1, &stmt, nullptr) != SQLITE_OK)
		return;

	sqlite3_bind_text(stmt, 1, pName, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, GameServer()->m_Account[ClientId].m_Username, -1, SQLITE_STATIC);

	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

// std::optional<CAccountSession> CAccounts::AccountInfoUsername(const char *pUsername)
//{
//	if(!m_AccDatabase)
//		return std::nullopt;
//
//	const char *SelectQuery = R"(
//		SELECT Username, RegisterDate, PlayerName, LastPlayerName, CurrentIP, LastIP, LoggedIn, LastLogin, Port, ClientId, Flags, VoteMenuPage Playtime, Deaths, Kills, Level, XP, Money, Inventory, LastActiveItems
//		FROM Accounts
//		WHERE Username;
//	)";
//
//	sqlite3_stmt *stmt;
//	if(sqlite3_prepare_v2(m_AccDatabase, SelectQuery, -1, &stmt, nullptr) != SQLITE_OK)
//		return std::nullopt;
//
//	sqlite3_bind_text(stmt, 1, pUsername, -1, SQLITE_STATIC);
//
//	if(sqlite3_step(stmt) == SQLITE_ROW)
//	{
//		CAccountSession Acc = CAccountSession();
//
//		str_copy(Acc.m_Username, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
//		Acc.m_RegisterDate = sqlite3_column_int64(stmt, 1);
//		str_copy(Acc.m_Name, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
//		str_copy(Acc.m_LastName, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
//		str_copy(Acc.CurrentIp, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
//		str_copy(Acc.LastIp, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5)));
//		Acc.m_LoggedIn = sqlite3_column_int(stmt, 6);
//		Acc.m_LastLogin = sqlite3_column_int64(stmt, 7);
//		Acc.m_Port = sqlite3_column_int(stmt, 8);
//		Acc.ClientId = sqlite3_column_int(stmt, 9);
//		Acc.m_Flags = sqlite3_column_int64(stmt, 10);
//		Acc.m_VoteMenuPage = sqlite3_column_int64(stmt, 11);
//		Acc.m_Playtime = sqlite3_column_int64(stmt, 12);
//		Acc.m_Deaths = sqlite3_column_int64(stmt, 13);
//		Acc.m_Kills = sqlite3_column_int64(stmt, 14);
//		Acc.m_Level = sqlite3_column_int64(stmt, 15);
//		Acc.m_XP = sqlite3_column_int64(stmt, 16);
//		Acc.m_Money = sqlite3_column_int64(stmt, 17);
//		str_copy(Acc.m_Inventory, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 18)));
//		str_copy(Acc.m_LastActiveItems, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 19)));
//
//		sqlite3_finalize(stmt);
//		return Acc;
//	}
//	sqlite3_finalize(stmt);
//
//	return std::nullopt;
//}