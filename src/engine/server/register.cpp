/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <engine/shared/network.h>
#include <engine/shared/config.h>
#include <engine/console.h>
#include <engine/masterserver.h>

#include <mastersrv/mastersrv.h>

#include "register.h"

CRegister::CRegister(bool Sixup)
{
	m_Sixup = Sixup;
	m_pName = Sixup ? "regsixup" : "register";
	m_LastTokenRequest = 0;

	m_pNetServer = 0;
	m_pMasterServer = 0;
	m_pConsole = 0;

	m_RegisterState = REGISTERSTATE_START;
	m_RegisterStateStart = 0;
	m_RegisterFirst = 1;
	m_RegisterCount = 0;

	mem_zero(m_aMasterserverInfo, sizeof(m_aMasterserverInfo));
	m_RegisterRegisteredServer = -1;
}

void CRegister::FeedToken(NETADDR Addr, SECURITY_TOKEN ResponseToken)
{
	Addr.port = 0;
	for(int i = 0; i < IMasterServer::MAX_MASTERSERVERS; i++)
	{
		NETADDR Addr2 = m_aMasterserverInfo[i].m_Addr;
		Addr2.port = 0;
		if(net_addr_comp(&Addr, &Addr2) == 0)
		{
			m_aMasterserverInfo[i].m_Token = ResponseToken;
			break;
		}
	}
}

void CRegister::RegisterNewState(int State)
{
	m_RegisterState = State;
	m_RegisterStateStart = time_get();
}

void CRegister::RegisterSendFwcheckresponse(NETADDR *pAddr, SECURITY_TOKEN ResponseToken)
{
	CNetChunk Packet;
	Packet.m_ClientID = -1;
	Packet.m_Address = *pAddr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;
	Packet.m_DataSize = sizeof(SERVERBROWSE_FWRESPONSE);
	Packet.m_pData = SERVERBROWSE_FWRESPONSE;
	if(m_Sixup)
		m_pNetServer->SendConnlessSixup(&Packet, ResponseToken);
	else
		m_pNetServer->Send(&Packet);
}

void CRegister::RegisterSendHeartbeat(NETADDR Addr, SECURITY_TOKEN ResponseToken)
{
	static unsigned char aData[sizeof(SERVERBROWSE_HEARTBEAT) + 2];
	unsigned short Port = g_Config.m_SvPort;
	CNetChunk Packet;

	mem_copy(aData, SERVERBROWSE_HEARTBEAT, sizeof(SERVERBROWSE_HEARTBEAT));

	Packet.m_ClientID = -1;
	Packet.m_Address = Addr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;
	Packet.m_DataSize = sizeof(SERVERBROWSE_HEARTBEAT) + 2;
	Packet.m_pData = &aData;

	// supply the set port that the master can use if it has problems
	if(g_Config.m_SvExternalPort)
		Port = g_Config.m_SvExternalPort;
	aData[sizeof(SERVERBROWSE_HEARTBEAT)] = Port >> 8;
	aData[sizeof(SERVERBROWSE_HEARTBEAT)+1] = Port&0xff;
	if(m_Sixup)
		m_pNetServer->SendConnlessSixup(&Packet, ResponseToken);
	else
		m_pNetServer->Send(&Packet);
}

void CRegister::RegisterSendCountRequest(NETADDR Addr, SECURITY_TOKEN ResponseToken)
{
	CNetChunk Packet;
	Packet.m_ClientID = -1;
	Packet.m_Address = Addr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;
	Packet.m_DataSize = sizeof(SERVERBROWSE_GETCOUNT);
	Packet.m_pData = SERVERBROWSE_GETCOUNT;
	if(m_Sixup)
		m_pNetServer->SendConnlessSixup(&Packet, ResponseToken);
	else
		m_pNetServer->Send(&Packet);
}

void CRegister::RegisterGotCount(CNetChunk *pChunk)
{
	unsigned char *pData = (unsigned char *)pChunk->m_pData;
	int Count = (pData[sizeof(SERVERBROWSE_COUNT)]<<8) | pData[sizeof(SERVERBROWSE_COUNT)+1];

	for(int i = 0; i < IMasterServer::MAX_MASTERSERVERS; i++)
	{
		if(net_addr_comp(&m_aMasterserverInfo[i].m_Addr, &pChunk->m_Address) == 0)
		{
			m_aMasterserverInfo[i].m_Count = Count;
			break;
		}
	}
}

void CRegister::Init(CNetServer *pNetServer, IEngineMasterServer *pMasterServer, IConsole *pConsole)
{
	m_pNetServer = pNetServer;
	m_pMasterServer = pMasterServer;
	m_pConsole = pConsole;
}

void CRegister::RegisterUpdate(int Nettype)
{
	int64 Now = time_get();
	int64 Freq = time_freq();

	if(!g_Config.m_SvRegister)
		return;

	m_pMasterServer->Update();

	if(m_Sixup && (m_RegisterState == REGISTERSTATE_HEARTBEAT || m_RegisterState == REGISTERSTATE_REGISTERED) && Now > m_LastTokenRequest+Freq*5)
	{
		m_pNetServer->SendTokenSixup(m_aMasterserverInfo[m_RegisterRegisteredServer].m_Addr, NET_SECURITY_TOKEN_UNKNOWN);
		m_LastTokenRequest = Now;
	}

	if(m_RegisterState == REGISTERSTATE_START)
	{
		m_RegisterCount = 0;
		m_RegisterFirst = 1;
		RegisterNewState(REGISTERSTATE_UPDATE_ADDRS);
		m_pMasterServer->RefreshAddresses(Nettype);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_pName, "refreshing ip addresses");
	}
	else if(m_RegisterState == REGISTERSTATE_UPDATE_ADDRS)
	{
		m_RegisterRegisteredServer = -1;

		if(!m_pMasterServer->IsRefreshing())
		{
			int i;
			for(i = 0; i < IMasterServer::MAX_MASTERSERVERS; i++)
			{
				if(!m_pMasterServer->IsValid(i))
				{
					m_aMasterserverInfo[i].m_Valid = 0;
					m_aMasterserverInfo[i].m_Count = 0;
					continue;
				}

				NETADDR Addr = m_pMasterServer->GetAddr(i);
				m_aMasterserverInfo[i].m_Addr = Addr;
				if(m_Sixup)
					m_aMasterserverInfo[i].m_Addr.port = 8283;
				m_aMasterserverInfo[i].m_Valid = 1;
				m_aMasterserverInfo[i].m_Count = -1;
				m_aMasterserverInfo[i].m_LastSend = 0;
				m_aMasterserverInfo[i].m_Token = NET_SECURITY_TOKEN_UNKNOWN;
			}

			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_pName, "fetching server counts");
			m_LastTokenRequest = Now;
			RegisterNewState(REGISTERSTATE_QUERY_COUNT);
		}
	}
	else if(m_RegisterState == REGISTERSTATE_QUERY_COUNT)
	{
		int Left = 0;
		for(int i = 0; i < IMasterServer::MAX_MASTERSERVERS; i++)
		{
			if(!m_aMasterserverInfo[i].m_Valid)
				continue;

			if(m_aMasterserverInfo[i].m_Count == -1)
			{
				Left++;
				if(m_aMasterserverInfo[i].m_LastSend+Freq < Now)
				{
					m_aMasterserverInfo[i].m_LastSend = Now;
					if(m_Sixup && m_aMasterserverInfo[i].m_Token == NET_SECURITY_TOKEN_UNKNOWN)
						m_pNetServer->SendTokenSixup(m_aMasterserverInfo[i].m_Addr, NET_SECURITY_TOKEN_UNKNOWN);
					else
						RegisterSendCountRequest(m_aMasterserverInfo[i].m_Addr, m_aMasterserverInfo[i].m_Token);
				}
			}
		}

		// check if we are done or timed out
		if(Left == 0 || Now > m_RegisterStateStart+Freq*5)
		{
			// choose server
			int Best = -1;
			int i;
			for(i = 0; i < IMasterServer::MAX_MASTERSERVERS; i++)
			{
				if(!m_aMasterserverInfo[i].m_Valid || m_aMasterserverInfo[i].m_Count == -1)
					continue;

				if(Best == -1 || m_aMasterserverInfo[i].m_Count < m_aMasterserverInfo[Best].m_Count)
					Best = i;
			}

			// server chosen
			m_RegisterRegisteredServer = Best;
			if(m_RegisterRegisteredServer == -1)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_pName, "WARNING: No master servers. Retrying in 60 seconds");
				RegisterNewState(REGISTERSTATE_ERROR);
			}
			else
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "chose '%s' as master, sending heartbeats", m_pMasterServer->GetName(m_RegisterRegisteredServer));
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_pName, aBuf);
				m_aMasterserverInfo[m_RegisterRegisteredServer].m_LastSend = 0;
				RegisterNewState(REGISTERSTATE_HEARTBEAT);
			}
		}
	}
	else if(m_RegisterState == REGISTERSTATE_HEARTBEAT)
	{
		// check if we should send heartbeat
		if(Now > m_aMasterserverInfo[m_RegisterRegisteredServer].m_LastSend+Freq*15)
		{
			m_aMasterserverInfo[m_RegisterRegisteredServer].m_LastSend = Now;
			RegisterSendHeartbeat(m_aMasterserverInfo[m_RegisterRegisteredServer].m_Addr, m_aMasterserverInfo[m_RegisterRegisteredServer].m_Token);
		}

		if(Now > m_RegisterStateStart+Freq*60)
		{
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_pName, "WARNING: Master server is not responding, switching master");
			RegisterNewState(REGISTERSTATE_START);
		}
	}
	else if(m_RegisterState == REGISTERSTATE_REGISTERED)
	{
		if(m_RegisterFirst)
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_pName, "server registered");

		m_RegisterFirst = 0;

		// check if we should send new heartbeat again
		if(Now > m_RegisterStateStart+Freq)
		{
			if(m_RegisterCount == 120) // redo the whole process after 60 minutes to balance out the master servers
				RegisterNewState(REGISTERSTATE_START);
			else
			{
				m_RegisterCount++;
				RegisterNewState(REGISTERSTATE_HEARTBEAT);
			}
		}
	}
	else if(m_RegisterState == REGISTERSTATE_ERROR)
	{
		// check for restart
		if(Now > m_RegisterStateStart+Freq*60)
			RegisterNewState(REGISTERSTATE_START);
	}
}

int CRegister::RegisterProcessPacket(CNetChunk *pPacket, SECURITY_TOKEN ResponseToken)
{
	// check for masterserver address
	bool Valid = false;
	for(int i = 0; i < IMasterServer::MAX_MASTERSERVERS; i++)
	{
		if(net_addr_comp_noport(&pPacket->m_Address, &m_aMasterserverInfo[i].m_Addr) == 0)
		{
			Valid = true;
			break;
		}
	}
	if(!Valid)
		return 0;

	if(pPacket->m_DataSize == sizeof(SERVERBROWSE_FWCHECK) &&
		mem_comp(pPacket->m_pData, SERVERBROWSE_FWCHECK, sizeof(SERVERBROWSE_FWCHECK)) == 0)
	{
		RegisterSendFwcheckresponse(&pPacket->m_Address, ResponseToken);
		return 1;
	}
	else if(pPacket->m_DataSize == sizeof(SERVERBROWSE_FWOK) &&
		mem_comp(pPacket->m_pData, SERVERBROWSE_FWOK, sizeof(SERVERBROWSE_FWOK)) == 0)
	{
		if(m_RegisterFirst)
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_pName, "no firewall/nat problems detected");

		if(m_RegisterState == REGISTERSTATE_HEARTBEAT || m_RegisterState == REGISTERSTATE_REGISTERED)
			RegisterNewState(REGISTERSTATE_REGISTERED);
		return 1;
	}
	else if(pPacket->m_DataSize == sizeof(SERVERBROWSE_FWERROR) &&
		mem_comp(pPacket->m_pData, SERVERBROWSE_FWERROR, sizeof(SERVERBROWSE_FWERROR)) == 0)
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_pName, "ERROR: the master server reports that clients can not connect to this server.");
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "ERROR: configure your firewall/nat to let through udp on port %d.", g_Config.m_SvPort);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_pName, aBuf);
		//RegisterNewState(REGISTERSTATE_ERROR);
		return 1;
	}
	else if(pPacket->m_DataSize == sizeof(SERVERBROWSE_COUNT)+2 &&
		mem_comp(pPacket->m_pData, SERVERBROWSE_COUNT, sizeof(SERVERBROWSE_COUNT)) == 0)
	{
		RegisterGotCount(pPacket);
		return 1;
	}

	return 0;
}
