#include <engine/console.h>
#include <engine/shared/config.h>

#include "econ.h"
#include "netban.h"

CEcon::CEcon() :
	m_Ready(false)
{
}

int CEcon::NewClientCallback(int ClientId, void *pUser)
{
	CEcon *pThis = (CEcon *)pUser;

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pThis->m_NetConsole.ClientAddr(ClientId), aAddrStr, sizeof(aAddrStr), true);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "client accepted. cid=%d addr=%s'", ClientId, aAddrStr);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "econ", aBuf);

	pThis->m_aClients[ClientId].m_State = CClient::STATE_CONNECTED;
	pThis->m_aClients[ClientId].m_TimeConnected = time_get();
	pThis->m_aClients[ClientId].m_AuthTries = 0;

	pThis->m_NetConsole.Send(ClientId, "Enter password:");
	return 0;
}

int CEcon::DelClientCallback(int ClientId, const char *pReason, void *pUser)
{
	CEcon *pThis = (CEcon *)pUser;

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pThis->m_NetConsole.ClientAddr(ClientId), aAddrStr, sizeof(aAddrStr), true);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "client dropped. cid=%d addr=%s reason='%s'", ClientId, aAddrStr, pReason);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "econ", aBuf);

	pThis->m_aClients[ClientId].m_State = CClient::STATE_EMPTY;
	return 0;
}

void CEcon::ConLogout(IConsole::IResult *pResult, void *pUserData)
{
	CEcon *pThis = static_cast<CEcon *>(pUserData);

	if(pThis->m_UserClientId >= 0 && pThis->m_UserClientId < NET_MAX_CONSOLE_CLIENTS && pThis->m_aClients[pThis->m_UserClientId].m_State != CClient::STATE_EMPTY)
		pThis->m_NetConsole.Drop(pThis->m_UserClientId, "Logout");
}

void CEcon::Init(CConfig *pConfig, IConsole *pConsole, CNetBan *pNetBan)
{
	m_pConfig = pConfig;
	m_pConsole = pConsole;

	for(auto &Client : m_aClients)
		Client.m_State = CClient::STATE_EMPTY;

	m_Ready = false;
	m_UserClientId = -1;

	if(g_Config.m_EcPort == 0)
		return;

	if(g_Config.m_EcPassword[0] == 0)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "econ", "ec_password is required to be set for econ to be enabled.");
		return;
	}

	NETADDR BindAddr;
	if(g_Config.m_EcBindaddr[0] && net_host_lookup(g_Config.m_EcBindaddr, &BindAddr, NETTYPE_ALL) == 0)
	{
		// got bindaddr
		BindAddr.port = g_Config.m_EcPort;
	}
	else
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "The configured bindaddr '%s' cannot be resolved.", g_Config.m_EcBindaddr);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "econ", aBuf);
		return;
	}

	if(m_NetConsole.Open(BindAddr, pNetBan))
	{
		m_NetConsole.SetCallbacks(NewClientCallback, DelClientCallback, this);
		m_Ready = true;
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "bound to %s:%d", g_Config.m_EcBindaddr, g_Config.m_EcPort);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "econ", aBuf);
		Console()->Register("logout", "", CFGFLAG_ECON, ConLogout, this, "Logout of econ");
	}
	else
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "econ", "couldn't open socket. port might already be in use");
}

void CEcon::Update()
{
	if(!m_Ready)
		return;

	m_NetConsole.Update();

	char aBuf[NET_MAX_PACKETSIZE];
	int ClientId;

	while(m_NetConsole.Recv(aBuf, (int)(sizeof(aBuf)) - 1, &ClientId))
	{
		dbg_assert(m_aClients[ClientId].m_State != CClient::STATE_EMPTY, "got message from empty slot");
		if(m_aClients[ClientId].m_State == CClient::STATE_CONNECTED)
		{
			if(str_comp(aBuf, g_Config.m_EcPassword) == 0)
			{
				m_aClients[ClientId].m_State = CClient::STATE_AUTHED;
				m_NetConsole.Send(ClientId, "Authentication successful. External console access granted.");

				str_format(aBuf, sizeof(aBuf), "cid=%d authed", ClientId);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "econ", aBuf);
			}
			else
			{
				m_aClients[ClientId].m_AuthTries++;
				char aMsg[128];
				str_format(aMsg, sizeof(aMsg), "Wrong password %d/%d.", m_aClients[ClientId].m_AuthTries, MAX_AUTH_TRIES);
				m_NetConsole.Send(ClientId, aMsg);
				if(m_aClients[ClientId].m_AuthTries >= MAX_AUTH_TRIES)
				{
					if(!g_Config.m_EcBantime)
						m_NetConsole.Drop(ClientId, "Too many authentication tries");
					else
						m_NetConsole.NetBan()->BanAddr(m_NetConsole.ClientAddr(ClientId), g_Config.m_EcBantime * 60, "Too many authentication tries", false);
				}
			}
		}
		else if(m_aClients[ClientId].m_State == CClient::STATE_AUTHED)
		{
			char aFormatted[256];
			str_format(aFormatted, sizeof(aFormatted), "cid=%d cmd='%s'", ClientId, aBuf);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aFormatted);
			m_UserClientId = ClientId;
			Console()->ExecuteLine(aBuf);
			m_UserClientId = -1;
		}
	}

	for(int i = 0; i < NET_MAX_CONSOLE_CLIENTS; ++i)
	{
		if(m_aClients[i].m_State == CClient::STATE_CONNECTED &&
			time_get() > m_aClients[i].m_TimeConnected + g_Config.m_EcAuthTimeout * time_freq())
			m_NetConsole.Drop(i, "authentication timeout");
	}
}

void CEcon::Send(int ClientId, const char *pLine)
{
	if(!m_Ready)
		return;

	if(ClientId == -1)
	{
		for(int i = 0; i < NET_MAX_CONSOLE_CLIENTS; i++)
		{
			if(m_aClients[i].m_State == CClient::STATE_AUTHED)
				m_NetConsole.Send(i, pLine);
		}
	}
	else if(ClientId >= 0 && ClientId < NET_MAX_CONSOLE_CLIENTS && m_aClients[ClientId].m_State == CClient::STATE_AUTHED)
		m_NetConsole.Send(ClientId, pLine);
}

void CEcon::Shutdown()
{
	if(!m_Ready)
		return;

	m_NetConsole.Close();
}
