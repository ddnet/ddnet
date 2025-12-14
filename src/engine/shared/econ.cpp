#include "econ.h"

#include "netban.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/net.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/shared/config.h>

CEcon::CEcon() :
	m_Ready(false)
{
}

int CEcon::NewClientCallback(int ClientId, void *pUser)
{
	CEcon *pThis = (CEcon *)pUser;

	log_info("econ", "Client accepted. client_id=%d addr=<{%s}>", ClientId, pThis->m_NetConsole.ClientAddrString(ClientId).data());

	pThis->m_aClients[ClientId].m_State = CClient::STATE_CONNECTED;
	pThis->m_aClients[ClientId].m_TimeConnected = time_get();
	pThis->m_aClients[ClientId].m_AuthTries = 0;

	pThis->m_NetConsole.Send(ClientId, "Enter password:");
	return 0;
}

int CEcon::DelClientCallback(int ClientId, const char *pReason, void *pUser)
{
	CEcon *pThis = (CEcon *)pUser;

	log_info("econ", "Client dropped. client_id=%d addr=<{%s}> reason='%s'", ClientId, pThis->m_NetConsole.ClientAddrString(ClientId).data(), pReason);

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
		log_error("econ", "Setting ec_password is required for econ to be enabled.");
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
		log_error("econ", "The configured bindaddr '%s' cannot be resolved.", g_Config.m_EcBindaddr);
		return;
	}

	if(m_NetConsole.Open(BindAddr, pNetBan))
	{
		m_NetConsole.SetCallbacks(NewClientCallback, DelClientCallback, this);
		m_Ready = true;
		log_info("econ", "Bound to %s:%d", g_Config.m_EcBindaddr, g_Config.m_EcPort);
		Console()->Register("logout", "", CFGFLAG_ECON, ConLogout, this, "Logout of econ");
	}
	else
	{
		log_error("econ", "Couldn't open socket. Port %d might already be in use.", g_Config.m_EcPort);
	}
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
				log_info("econ", "Client authed. client_id=%d addr=<{%s}>", ClientId, m_NetConsole.ClientAddrString(ClientId).data());
			}
			else
			{
				m_aClients[ClientId].m_AuthTries++;
				char aMsg[64];
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
			log_info("econ", "client_id=%d addr=<{%s}> command='%s'", ClientId, m_NetConsole.ClientAddrString(ClientId).data(), aBuf);
			m_UserClientId = ClientId;
			Console()->ExecuteLine(aBuf, IConsole::CLIENT_ID_UNSPECIFIED);
			m_UserClientId = -1;
		}
	}

	for(int i = 0; i < NET_MAX_CONSOLE_CLIENTS; ++i)
	{
		if(m_aClients[i].m_State == CClient::STATE_CONNECTED &&
			time_get() > m_aClients[i].m_TimeConnected + g_Config.m_EcAuthTimeout * time_freq())
		{
			m_NetConsole.Drop(i, "Authentication timeout");
		}
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
