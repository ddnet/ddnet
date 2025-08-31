#include "accountworker.h"
#include <engine/server/databases/connection.h>
#include "accounts.h"

bool CAccountsWorker::Register(IDbConnection *pSql, const ISqlData *pData, Write w, char *pError, int ErrorSize)
{
	const auto *pReq = dynamic_cast<const CAccRegisterRequest *>(pData);
	auto *pRes = dynamic_cast<CAccResult *>(pData->m_pResult.get());

	char aSql[512];
	str_copy(aSql, "INSERT INTO foxnet_accounts (Username, Password, RegisterDate, Flags) VALUES (?, ?, ?, ?)", sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;

	pSql->BindString(1, pReq->m_Username);
	pSql->BindString(2, pReq->m_PasswordHash);
	pSql->BindInt64(3, pReq->m_RegisterDate);
	pSql->BindInt64(4, ACC_FLAG_AUTOLOGIN);

	int NumUpdated = 0;
	if(!pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
		return false;

	pRes->m_Success = NumUpdated == 1;
	pRes->m_Completed.store(true);
	return true;
}

bool CAccountsWorker::Login(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize)
{
	const auto *pReq = dynamic_cast<const CAccLoginRequest *>(pData);
	auto *pRes = dynamic_cast<CAccResult *>(pData->m_pResult.get());

	char aSql[1024];
	str_copy(aSql,
		"SELECT Username, RegisterDate, PlayerName, LastPlayerName, CurrentIP, LastIP, "
		"LoggedIn, LastLogin, Port, ClientId, Flags, VoteMenuPage, Playtime, Deaths, Kills, "
		"Level, XP, Money, Inventory, LastActiveItems "
		"FROM foxnet_accounts WHERE Username = ? AND Password = ?",
		sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;

	pSql->BindString(1, pReq->m_Username);
	pSql->BindString(2, pReq->m_PasswordHash);

	bool End = true;
	if(!pSql->Step(&End, pError, ErrorSize))
		return false;

	if(!End)
	{
		pSql->GetString(1, pRes->m_Username, sizeof(pRes->m_Username));
		pRes->m_RegisterDate = pSql->GetInt64(2);
		pSql->GetString(3, pRes->m_PlayerName, sizeof(pRes->m_PlayerName));
		pSql->GetString(4, pRes->m_LastPlayerName, sizeof(pRes->m_LastPlayerName));
		pSql->GetString(5, pRes->m_CurrentIP, sizeof(pRes->m_CurrentIP));
		pSql->GetString(6, pRes->m_LastIP, sizeof(pRes->m_LastIP));
		pRes->m_LoggedIn = pSql->GetInt(7);
		pRes->m_LastLogin = pSql->GetInt64(8);
		pRes->m_Port = pSql->GetInt(9);
		pRes->m_ClientId = pSql->GetInt(10);
		pRes->m_Flags = pSql->GetInt64(11);
		pRes->m_VoteMenuPage = pSql->GetInt(12);
		pRes->m_Playtime = pSql->GetInt64(13);
		pRes->m_Deaths = pSql->GetInt64(14);
		pRes->m_Kills = pSql->GetInt64(15);
		pRes->m_Level = pSql->GetInt64(16);
		pRes->m_XP = pSql->GetInt64(17);
		pRes->m_Money = pSql->GetInt64(18);
		pSql->GetString(19, pRes->m_Inventory, sizeof(pRes->m_Inventory));
		pSql->GetString(20, pRes->m_LastActiveItems, sizeof(pRes->m_LastActiveItems));
		pRes->m_Found = true; 
		pRes->m_Success = true;
	}
	pRes->m_Completed.store(true);
	return true;
}

bool CAccountsWorker::UpdateLoginState(IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize)
{
	const auto *p = dynamic_cast<const CAccUpdLoginState *>(pData);
	char aSql[512];
	str_copy(aSql,
		"UPDATE foxnet_accounts "
		"SET LastPlayerName = PlayerName, "
		"    PlayerName = ?, "
		"    LastIP = CurrentIP, "
		"    CurrentIP = ?, "
		"    LoggedIn = 1, "
		"    LastLogin = ?, "
		"    Port = ?, "
		"    ClientId = ? "
		"WHERE Username = ?",
		sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;
	pSql->BindString(1, p->m_PlayerName);
	pSql->BindString(2, p->m_CurrentIP);
	pSql->BindInt64(3, p->m_LastLogin);
	pSql->BindInt(4, p->m_Port);
	pSql->BindInt(5, p->m_ClientId);
	pSql->BindString(6, p->m_Username);
	int NumUpdated = 0;
	return pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
}

bool CAccountsWorker::UpdateLogoutState(IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize)
{
	const auto *p = dynamic_cast<const CAccUpdLogoutState *>(pData);
	char aSql[768];
	str_copy(aSql,
		"UPDATE foxnet_accounts "
		"SET LoggedIn = 0, Port = 0, ClientId = -1, "
		"    LastPlayerName = PlayerName, LastIP = CurrentIP, "
		"    Flags = ?, VoteMenuPage = ?, Playtime = ?, Deaths = ?, Kills = ?, "
		"    Level = ?, XP = ?, Money = ?, Inventory = ?, LastActiveItems = ? "
		"WHERE Username = ?",
		sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;
	pSql->BindInt64(1, p->m_Flags);
	pSql->BindInt(2, p->m_VoteMenuPage);
	pSql->BindInt64(3, p->m_Playtime);
	pSql->BindInt64(4, p->m_Deaths);
	pSql->BindInt64(5, p->m_Kills);
	pSql->BindInt64(6, p->m_Level);
	pSql->BindInt64(7, p->m_XP);
	pSql->BindInt64(8, p->m_Money);
	pSql->BindString(9, p->m_Inventory);
	pSql->BindString(10, p->m_LastActiveItems);
	pSql->BindString(11, p->m_Username);
	int NumUpdated = 0;
	return pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
}

bool CAccountsWorker::SelectByLastPlayerName(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize)
{
	const auto *p = dynamic_cast<const CAccSelectByLastName *>(pData);
	auto *pRes = dynamic_cast<CAccResult *>(pData->m_pResult.get());

	char aSql[1024];
	str_copy(aSql,
		"SELECT Username, RegisterDate, PlayerName, LastPlayerName, CurrentIP, LastIP, "
		"LoggedIn, LastLogin, Port, ClientId, Flags, VoteMenuPage, Playtime, Deaths, Kills, "
		"Level, XP, Money, Inventory, LastActiveItems "
		"FROM foxnet_accounts WHERE LastPlayerName = ?",
		sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;
	pSql->BindString(1, p->m_LastPlayerName);

	bool End = true;
	if(!pSql->Step(&End, pError, ErrorSize))
		return false;

	if(!End)
	{
		pSql->GetString(1, pRes->m_Username, sizeof(pRes->m_Username));
		pRes->m_RegisterDate = pSql->GetInt64(2);
		pSql->GetString(3, pRes->m_PlayerName, sizeof(pRes->m_PlayerName));
		pSql->GetString(4, pRes->m_LastPlayerName, sizeof(pRes->m_LastPlayerName));
		pSql->GetString(5, pRes->m_CurrentIP, sizeof(pRes->m_CurrentIP));
		pSql->GetString(6, pRes->m_LastIP, sizeof(pRes->m_LastIP));
		pRes->m_LoggedIn = pSql->GetInt(7);
		pRes->m_LastLogin = pSql->GetInt64(8);
		pRes->m_Port = pSql->GetInt(9);
		pRes->m_ClientId = pSql->GetInt(10);
		pRes->m_Flags = pSql->GetInt64(11);
		pRes->m_VoteMenuPage = pSql->GetInt(12);
		pRes->m_Playtime = pSql->GetInt64(13);
		pRes->m_Deaths = pSql->GetInt64(14);
		pRes->m_Kills = pSql->GetInt64(15);
		pRes->m_Level = pSql->GetInt64(16);
		pRes->m_XP = pSql->GetInt64(17);
		pRes->m_Money = pSql->GetInt64(18);
		pSql->GetString(19, pRes->m_Inventory, sizeof(pRes->m_Inventory));
		pSql->GetString(20, pRes->m_LastActiveItems, sizeof(pRes->m_LastActiveItems));
		pRes->m_Found = true;
		pRes->m_Success = true;
	}
	pRes->m_Completed.store(true);
	return true;
}

bool CAccountsWorker::SaveInfo(IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize)
{
	const auto *p = dynamic_cast<const CAccSaveInfo *>(pData);
	char aSql[768];
	str_copy(aSql,
		"UPDATE foxnet_accounts "
		"SET LastPlayerName = PlayerName, LastIP = CurrentIP, "
		"    Flags = ?, VoteMenuPage = ?, Playtime = ?, Deaths = ?, Kills = ?, "
		"    Level = ?, XP = ?, Money = ?, Inventory = ?, LastActiveItems = ? "
		"WHERE Username = ?",
		sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;
	pSql->BindInt64(1, p->m_Flags);
	pSql->BindInt(2, p->m_VoteMenuPage);
	pSql->BindInt64(3, p->m_Playtime);
	pSql->BindInt64(4, p->m_Deaths);
	pSql->BindInt64(5, p->m_Kills);
	pSql->BindInt64(6, p->m_Level);
	pSql->BindInt64(7, p->m_XP);
	pSql->BindInt64(8, p->m_Money);
	pSql->BindString(9, p->m_Inventory);
	pSql->BindString(10, p->m_LastActiveItems);
	pSql->BindString(11, p->m_Username);
	int NumUpdated = 0;
	return pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
}

bool CAccountsWorker::SetPlayerName(IDbConnection *pSql, const ISqlData *pData, Write, char *pError, int ErrorSize)
{
	const auto *p = dynamic_cast<const CAccSetNameReq *>(pData);
	char aSql[256];
	str_copy(aSql,
		"UPDATE foxnet_accounts SET LastPlayerName = PlayerName, PlayerName = ? WHERE Username = ?",
		sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;
	pSql->BindString(1, p->m_NewPlayerName);
	pSql->BindString(2, p->m_Username);
	int NumUpdated = 0;
	return pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
}

bool CAccountsWorker::SelectByUsername(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize)
{
	const auto *p = dynamic_cast<const CAccSelectByUser *>(pData);
	auto *pRes = dynamic_cast<CAccResult *>(pData->m_pResult.get());

	char aSql[1024];
	str_copy(aSql,
		"SELECT Username, RegisterDate, PlayerName, LastPlayerName, CurrentIP, LastIP, "
		"LoggedIn, LastLogin, Port, ClientId, Flags, VoteMenuPage, Playtime, Deaths, Kills, "
		"Level, XP, Money, Inventory, LastActiveItems "
		"FROM foxnet_accounts WHERE Username = ?",
		sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;
	pSql->BindString(1, p->m_Username);

	bool End = true;
	if(!pSql->Step(&End, pError, ErrorSize))
		return false;

	if(!End)
	{
		pSql->GetString(1, pRes->m_Username, sizeof(pRes->m_Username));
		pRes->m_RegisterDate = pSql->GetInt64(2);
		pSql->GetString(3, pRes->m_PlayerName, sizeof(pRes->m_PlayerName));
		pSql->GetString(4, pRes->m_LastPlayerName, sizeof(pRes->m_LastPlayerName));
		pSql->GetString(5, pRes->m_CurrentIP, sizeof(pRes->m_CurrentIP));
		pSql->GetString(6, pRes->m_LastIP, sizeof(pRes->m_LastIP));
		pRes->m_LoggedIn = pSql->GetInt(7);
		pRes->m_LastLogin = pSql->GetInt64(8);
		pRes->m_Port = pSql->GetInt(9);
		pRes->m_ClientId = pSql->GetInt(10);
		pRes->m_Flags = pSql->GetInt64(11);
		pRes->m_VoteMenuPage = pSql->GetInt(12);
		pRes->m_Playtime = pSql->GetInt64(13);
		pRes->m_Deaths = pSql->GetInt64(14);
		pRes->m_Kills = pSql->GetInt64(15);
		pRes->m_Level = pSql->GetInt64(16);
		pRes->m_XP = pSql->GetInt64(17);
		pRes->m_Money = pSql->GetInt64(18);
		pSql->GetString(19, pRes->m_Inventory, sizeof(pRes->m_Inventory));
		pSql->GetString(20, pRes->m_LastActiveItems, sizeof(pRes->m_LastActiveItems));
		pRes->m_Found = true;
		pRes->m_Success = true;
	}
	pRes->m_Completed.store(true);
	return true;
}


bool CAccountsWorker::SelectPortByUsername(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize)
{
	const auto *p = dynamic_cast<const CAccSelectPortByUser *>(pData);
	auto *pRes = dynamic_cast<CAccResult *>(pData->m_pResult.get());

	char aSql[256];
	str_copy(aSql, "SELECT Port FROM foxnet_accounts WHERE Username = ?", sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;
	pSql->BindString(1, p->m_Username);

	bool End = true;
	if(!pSql->Step(&End, pError, ErrorSize))
		return false;
	if(!End)
	{
		pRes->m_Port = pSql->GetInt(1);
		pRes->m_Success = true;
	}
	pRes->m_Completed.store(true);
	return true;
}