#include <engine/server/server.h>

#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>

#include <base/math.h>
#include <base/system.h>
#include <base/types.h>

#include <engine/console.h>
#include <engine/storage.h>

#include <utility>
#include <memory>

#include "bans.h"
#include <algorithm>

void CFoxNetBans::Init(IConsole *pConsole, IStorage *pStorage, CServer *pServer)
{
	m_pConsole = pConsole;
	m_pStorage = pStorage;
	m_pServer = pServer;
}

void CFoxNetBans::Ban(const NETADDR *pAddr, int64_t Duration, const char *pIssuer, const char *pPlayerName, const char *pReason)
{
	if(!m_pServer->DbPool())
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bans", "no database connection available");
		return;
	}

	auto pReq = std::make_unique<CBanIp>();
	net_addr_str(pAddr, pReq->m_aIP, sizeof(pReq->m_aIP), false);
	pReq->m_EndTimestamp = time_timestamp() + Duration;
	str_copy(pReq->m_aIssuer, pIssuer, sizeof(pReq->m_aIssuer));
	str_copy(pReq->m_aPlayerName, pPlayerName, sizeof(pReq->m_aPlayerName));
	str_copy(pReq->m_aReason, pReason, sizeof(pReq->m_aReason));
	pReq->m_pServer = m_pServer;

	// Use write execution so it goes through backup + proper write connection.
	m_pServer->DbPool()->Execute(BanIp, std::move(pReq), "ban ip");

	// Drop any already connected clients matching this IP (mirror logic from BanExt).
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_pServer->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
			continue;
		if(!net_addr_comp(m_pServer->ClientAddr(i), pAddr))
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "%s", pReason);
			m_pServer->m_NetServer.Drop(i, aBuf);
		}
	}
}

bool CFoxNetBans::BanIp(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize)
{
	const auto *pReq = dynamic_cast<const CBanIp *>(pData);
	if(!pReq || !pReq->m_pServer)
		return false;

	char aSql[256];
	str_copy(aSql, "REPLACE INTO foxnet_bans (IP, EndTimestamp, Issuer, PlayerName, Reason) VALUES (?, ?, ?, ?, ?)", sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;

	pSql->BindString(1, pReq->m_aIP);
	pSql->BindInt64(2, pReq->m_EndTimestamp);
	pSql->BindString(3, pReq->m_aIssuer);
	pSql->BindString(4, pReq->m_aPlayerName);
	pSql->BindString(5, pReq->m_aReason);

	int NumUpdated = 0;
	if(!pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
		return false;
	return true;
}

void CFoxNetBans::Unban(const NETADDR *pAddr)
{
	if(!m_pServer->DbPool())
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bans", "no database connection available");
		return;
	}
	auto pReq = std::make_unique<CBanIp>();
	net_addr_str(pAddr, pReq->m_aIP, sizeof(pReq->m_aIP), false);
	pReq->m_pServer = m_pServer;
	m_pServer->DbPool()->Execute(UnbanIp, std::move(pReq), "unban ip");
}

bool CFoxNetBans::UnbanIp(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize)
{
	const auto *pReq = dynamic_cast<const CBanIp *>(pData);
	if(!pReq || !pReq->m_pServer)
		return false;
	char aSql[128];
	str_copy(aSql, "DELETE FROM foxnet_bans WHERE IP = ?", sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;
	pSql->BindString(1, pReq->m_aIP);
	int NumUpdated = 0;
	if(!pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
		return false;
	return true;
}

void CFoxNetBans::CheckBanned(const NETADDR *pAddr)
{
	if(!m_pServer->DbPool())
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bans", "no database connection available");
		return;
	}
	auto pReq = std::make_unique<CCheckBanned>();
	net_addr_str(pAddr, pReq->m_aIP, sizeof(pReq->m_aIP), false);
	pReq->m_pServer = m_pServer;
	m_pServer->DbPool()->Execute(CheckBannedIp, std::move(pReq), "check banned ip");
}

bool CFoxNetBans::CheckBannedIp(IDbConnection *pSql, const ISqlData *pData, char *pError, int ErrorSize)
{
	const auto *pReq = dynamic_cast<const CCheckBanned *>(pData);
	if(!pReq || !pReq->m_pServer)
		return false;

	// Look up ban entry.
	char aSql[128];
	str_copy(aSql, "SELECT EndTimestamp, Reason FROM foxnet_bans WHERE IP = ?", sizeof(aSql));
	if(!pSql->PrepareStatement(aSql, pError, ErrorSize))
		return false;
	pSql->BindString(1, pReq->m_aIP);

	bool End = true;
	if(!pSql->Step(&End, pError, ErrorSize))
		return false;

	if(End) // Not banned.
		return true;

	int64_t EndTimestamp = pSql->GetInt64(1);
	char aReason[128] = "";
	pSql->GetString(2, aReason, sizeof(aReason));

	// Expired? Clean it up and allow.
	if(EndTimestamp > 0 && EndTimestamp <= time_timestamp())
	{
		char aDel[128];
		str_copy(aDel, "DELETE FROM foxnet_bans WHERE IP = ?", sizeof(aDel));
		if(!pSql->PrepareStatement(aDel, pError, ErrorSize))
			return false;
		pSql->BindString(1, pReq->m_aIP);
		int NumUpdated = 0;
		if(!pSql->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
			return false;
		return true;
	}

	// Still banned: drop any matching connected clients.
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(pReq->m_pServer->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
			continue;
		const char *pClientAddr = pReq->m_pServer->ClientAddrString(i, false);
		if(!str_comp(pClientAddr, pReq->m_aIP))
		{
			char aDropMsg[256];
			if(aReason[0])
				str_copy(aDropMsg, aReason, sizeof(aDropMsg));
			else
				str_copy(aDropMsg, "You have been banned.", sizeof(aDropMsg));
			pReq->m_pServer->m_NetServer.Drop(i, aDropMsg);
		}
	}
	return true;
}
