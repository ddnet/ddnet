/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>

#include "netban.h"
#include "network.h"

bool CNetConsole::Open(NETADDR BindAddr, CNetBan *pNetBan)
{
	// zero out the whole structure
	*this = CNetConsole{};
	m_pNetBan = pNetBan;

	m_Socket = net_tcp_create(BindAddr);
	if(!m_Socket)
	{
		return false;
	}
	if(net_tcp_listen(m_Socket, NET_MAX_CONSOLE_CLIENTS) != 0 ||
		net_set_non_blocking(m_Socket) != 0)
	{
		net_tcp_close(m_Socket);
		m_Socket = nullptr;
		return false;
	}

	for(auto &Slot : m_aSlots)
	{
		Slot.m_Connection.Reset();
	}

	return true;
}

void CNetConsole::SetCallbacks(NETFUNC_NEWCLIENT_CON pfnNewClient, NETFUNC_DELCLIENT pfnDelClient, void *pUser)
{
	m_pfnNewClient = pfnNewClient;
	m_pfnDelClient = pfnDelClient;
	m_pUser = pUser;
}

void CNetConsole::Close()
{
	if(!m_Socket)
	{
		return;
	}
	for(auto &Slot : m_aSlots)
	{
		Slot.m_Connection.Disconnect("closing console");
	}
	net_tcp_close(m_Socket);
	m_Socket = nullptr;
}

void CNetConsole::Drop(int ClientId, const char *pReason)
{
	if(m_pfnDelClient)
		m_pfnDelClient(ClientId, pReason, m_pUser);

	m_aSlots[ClientId].m_Connection.Disconnect(pReason);
}

int CNetConsole::AcceptClient(NETSOCKET Socket, const NETADDR *pAddr)
{
	const auto &DropClient = [&](const char *pError) {
		net_tcp_send(Socket, pError, str_length(pError));
		net_tcp_close(Socket);
	};

	// check if address is banned
	char aBanMessage[256];
	if(NetBan() && NetBan()->IsBanned(pAddr, aBanMessage, sizeof(aBanMessage)))
	{
		DropClient(aBanMessage);
		return -1;
	}

	// look for free slot or multiple clients with same address
	int FreeSlot = -1;
	for(int i = 0; i < NET_MAX_CONSOLE_CLIENTS; i++)
	{
		if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_OFFLINE)
		{
			if(FreeSlot == -1)
			{
				FreeSlot = i;
			}
		}
		else if(net_addr_comp_noport(pAddr, m_aSlots[i].m_Connection.PeerAddress()) == 0)
		{
			DropClient("only one client per IP allowed");
			return -1;
		}
	}

	if(FreeSlot == -1)
	{
		DropClient("no free slot available");
		return -1;
	}

	// accept client
	if(m_aSlots[FreeSlot].m_Connection.Init(Socket, pAddr) != 0)
	{
		DropClient("failed to initialize client connection");
		return -1;
	}
	if(m_pfnNewClient)
	{
		m_pfnNewClient(FreeSlot, m_pUser);
	}
	return 0;
}

void CNetConsole::Update()
{
	NETSOCKET Socket;
	NETADDR Addr;
	if(net_tcp_accept(m_Socket, &Socket, &Addr) > 0)
	{
		AcceptClient(Socket, &Addr);
	}

	for(int i = 0; i < NET_MAX_CONSOLE_CLIENTS; i++)
	{
		if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_ONLINE)
			m_aSlots[i].m_Connection.Update();
		if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_ERROR)
			Drop(i, m_aSlots[i].m_Connection.ErrorString());
	}
}

int CNetConsole::Recv(char *pLine, int MaxLength, int *pClientId)
{
	for(int i = 0; i < NET_MAX_CONSOLE_CLIENTS; i++)
	{
		if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_ONLINE && m_aSlots[i].m_Connection.Recv(pLine, MaxLength))
		{
			if(pClientId)
				*pClientId = i;
			return 1;
		}
	}
	return 0;
}

int CNetConsole::Send(int ClientId, const char *pLine)
{
	if(m_aSlots[ClientId].m_Connection.State() == NET_CONNSTATE_ONLINE)
		return m_aSlots[ClientId].m_Connection.Send(pLine);
	else
		return -1;
}
