#include <engine/client/client.h>

int CClient::TranslateSysMsg(int *pMsgId, bool System, CUnpacker *pUnpacker, CPacker *pPacker, CNetChunk *pPacket, bool *pIsExMsg)
{
	*pIsExMsg = false;
	if(!System)
		return -1;

	// ddnet ex
	if(*pMsgId > NETMSG_WHATIS && *pMsgId < NETMSG_RCON_CMD_GROUP_END)
	{
		*pIsExMsg = true;
		return 0;
	}

	pPacker->Reset();

	if(*pMsgId == protocol7::NETMSG_MAP_CHANGE)
	{
		*pMsgId = NETMSG_MAP_CHANGE;
		const char *pMapName = pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES);
		int MapCrc = pUnpacker->GetInt();
		int Size = pUnpacker->GetInt();
		m_TranslationContext.m_MapDownloadChunksPerRequest = pUnpacker->GetInt();
		int ChunkSize = pUnpacker->GetInt();
		// void *pSha256 = pUnpacker->GetRaw(); // probably safe to ignore
		pPacker->AddString(pMapName, 0);
		pPacker->AddInt(MapCrc);
		pPacker->AddInt(Size);
		m_TranslationContext.m_MapdownloadTotalsize = Size;
		m_TranslationContext.m_MapDownloadChunkSize = ChunkSize;
		return 0;
	}
	else if(*pMsgId == protocol7::NETMSG_SERVERINFO)
	{
		// side effect only
		// this is a 0.7 only message and not handled in 0.6 code
		*pMsgId = -1;
		net_addr_str(&pPacket->m_Address, m_CurrentServerInfo.m_aAddress, sizeof(m_CurrentServerInfo.m_aAddress), true);
		str_copy(m_CurrentServerInfo.m_aVersion, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES));
		str_copy(m_CurrentServerInfo.m_aName, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES));
		str_clean_whitespaces(m_CurrentServerInfo.m_aName);
		pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES); // Hostname
		str_copy(m_CurrentServerInfo.m_aMap, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES));
		str_copy(m_CurrentServerInfo.m_aGameType, pUnpacker->GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES));
		int Flags = pUnpacker->GetInt();
		if(Flags & SERVER_FLAG_PASSWORD)
			m_CurrentServerInfo.m_Flags |= SERVER_FLAG_PASSWORD;
		// ddnets http master server handles timescore for us already
		// if(Flags&SERVER_FLAG_TIMESCORE)
		// 	m_CurrentServerInfo.m_Flags |= SERVER_FLAG_TIMESCORE;
		pUnpacker->GetInt(); // Server level
		m_CurrentServerInfo.m_NumPlayers = pUnpacker->GetInt();
		m_CurrentServerInfo.m_MaxPlayers = pUnpacker->GetInt();
		m_CurrentServerInfo.m_NumClients = pUnpacker->GetInt();
		m_CurrentServerInfo.m_MaxClients = pUnpacker->GetInt();
		return 0;
	}
	else if(*pMsgId == protocol7::NETMSG_RCON_AUTH_ON)
	{
		*pMsgId = NETMSG_RCON_AUTH_STATUS;
		pPacker->AddInt(1); // authed
		pPacker->AddInt(1); // cmdlist
		return 0;
	}
	else if(*pMsgId == protocol7::NETMSG_RCON_AUTH_OFF)
	{
		*pMsgId = NETMSG_RCON_AUTH_STATUS;
		pPacker->AddInt(0); // authed
		pPacker->AddInt(0); // cmdlist
		return 0;
	}
	else if(*pMsgId == protocol7::NETMSG_MAP_DATA)
	{
		// not binary compatible but translation happens on unpack
		*pMsgId = NETMSG_MAP_DATA;
	}
	else if(*pMsgId >= protocol7::NETMSG_CON_READY && *pMsgId <= protocol7::NETMSG_INPUTTIMING)
	{
		*pMsgId = *pMsgId - 1;
	}
	else if(*pMsgId == protocol7::NETMSG_RCON_LINE)
	{
		*pMsgId = NETMSG_RCON_LINE;
	}
	else if(*pMsgId == protocol7::NETMSG_RCON_CMD_ADD)
	{
		*pMsgId = NETMSG_RCON_CMD_ADD;
	}
	else if(*pMsgId == protocol7::NETMSG_RCON_CMD_REM)
	{
		*pMsgId = NETMSG_RCON_CMD_REM;
	}
	else if(*pMsgId == protocol7::NETMSG_PING_REPLY)
	{
		*pMsgId = NETMSG_PING_REPLY;
	}
	else if(*pMsgId == protocol7::NETMSG_MAPLIST_ENTRY_ADD || *pMsgId == protocol7::NETMSG_MAPLIST_ENTRY_REM)
	{
		// This is just a nice to have so silently dropping that is fine
		return -1;
	}
	else if(*pMsgId >= NETMSG_INFO && *pMsgId <= NETMSG_MAP_DATA)
	{
		return -1; // same in 0.6 and 0.7
	}
	else if(*pMsgId < OFFSET_UUID)
	{
		dbg_msg("sixup", "drop unknown sys msg=%d", *pMsgId);
		return -1;
	}

	return -1;
}
