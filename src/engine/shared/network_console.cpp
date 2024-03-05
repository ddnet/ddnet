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

	// open socket
	m_Socket = net_tcp_create(BindAddr);
	if(!m_Socket)
		return false;
	if(net_tcp_listen(m_Socket, NET_MAX_CONSOLE_CLIENTS))
		return false;
	net_set_non_blocking(m_Socket);

	for(auto &Slot : m_aSlots)
		Slot.m_Connection.Reset();

	return true;
}

void CNetConsole::SetCallbacks(NETFUNC_NEWCLIENT_CON pfnNewClient, NETFUNC_DELCLIENT pfnDelClient, void *pUser)
{
	m_pfnNewClient = pfnNewClient;
	m_pfnDelClient = pfnDelClient;
	m_pUser = pUser;
}

int CNetConsole::Close()
{
	for(auto &Slot : m_aSlots)
		Slot.m_Connection.Disconnect("closing console");

	net_tcp_close(m_Socket);

	return 0;
}

int CNetConsole::Drop(int ClientId, const char *pReason)
{
	if(m_pfnDelClient)
		m_pfnDelClient(ClientId, pReason, m_pUser);

	m_aSlots[ClientId].m_Connection.Disconnect(pReason);

	return 0;
}

int CNetConsole::AcceptClient(NETSOCKET Socket, const NETADDR *pAddr)
{
	char aError[256] = {0};
	int FreeSlot = -1;

	// look for free slot or multiple client
	for(int i = 0; i < NET_MAX_CONSOLE_CLIENTS; i++)
	{
		if(FreeSlot == -1 && m_aSlots[i].m_Connection.State() == NET_CONNSTATE_OFFLINE)
			FreeSlot = i;
		if(m_aSlots[i].m_Connection.State() != NET_CONNSTATE_OFFLINE)
		{
			if(net_addr_comp(pAddr, m_aSlots[i].m_Connection.PeerAddress()) == 0)
			{
				str_copy(aError, "only one client per IP allowed");
				break;
			}
		}
	}

	// accept client
	if(!aError[0] && FreeSlot != -1)
	{
		m_aSlots[FreeSlot].m_Connection.Init(Socket, pAddr);
		if(m_pfnNewClient)
			m_pfnNewClient(FreeSlot, m_pUser);
		return 0;
	}

	// reject client
	if(!aError[0])
		str_copy(aError, "no free slot available");

	net_tcp_send(Socket, aError, str_length(aError));
	net_tcp_close(Socket);

	return -1;
}

int CNetConsole::Update()
{
	NETSOCKET Socket;
	NETADDR Addr;

	if(net_tcp_accept(m_Socket, &Socket, &Addr) > 0)
	{
		// check if we just should drop the packet
		char aBuf[128];
		if(NetBan() && NetBan()->IsBanned(&Addr, aBuf, sizeof(aBuf)))
		{
			// banned, reply with a message and drop
			net_tcp_send(Socket, aBuf, str_length(aBuf));
			net_tcp_close(Socket);
		}
		else
			AcceptClient(Socket, &Addr);
	}

	for(int i = 0; i < NET_MAX_CONSOLE_CLIENTS; i++)
	{
		if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_ONLINE)
			m_aSlots[i].m_Connection.Update();
		if(m_aSlots[i].m_Connection.State() == NET_CONNSTATE_ERROR)
			Drop(i, m_aSlots[i].m_Connection.ErrorString());
	}

	return 0;
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
