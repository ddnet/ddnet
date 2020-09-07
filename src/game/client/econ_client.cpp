#include "econ_client.h"

#include <iostream>
#include <game/client/components/console.h>

void CEconClient::Init(CGameConsole *pGameConsole)
{
	m_pGameConsole = pGameConsole;
	m_Connection.Reset();
}

void CEconClient::Connect(const char *pAddress, const char *pPassword)
{
	NETADDR BindAddr;
	mem_zero(&BindAddr, sizeof(BindAddr));
	BindAddr.type = NETTYPE_IPV4;
	do
	{
		BindAddr.port = (secure_rand() % 64511) + 1024;
		m_Socket = net_tcp_create(BindAddr);
	}
	while(!m_Socket.type);
	NETADDR Addr;
	if(net_host_lookup(pAddress, &Addr, NETTYPE_IPV4) == 0)
	{
		std::cout << "CONNECTING TO " << pAddress << std::endl;
		m_Connection.Init(m_Socket, &Addr);
	}
}

void CEconClient::Disconnect(const char *pReason)
{
	m_Connection.Disconnect(pReason);
	net_tcp_close(m_Socket);
}

void CEconClient::Send(const char *pLine)
{
	if(m_Connection.State() != NET_CONNSTATE_ONLINE)
	{
		m_pGameConsole->PrintLine(CGameConsole::CONSOLETYPE_ECON, "not connected");
		return;
	}
	if(m_State < STATE_AUTHED)
	{
		m_pGameConsole->PrintLine(CGameConsole::CONSOLETYPE_ECON, "not authenticated");
		return;
	}

	m_Connection.Send(pLine);
}

void CEconClient::Update()
{
	m_Connection.Update();

	char aBuf[NET_MAX_PACKETSIZE];
	while(m_Connection.Recv(aBuf, (int)(sizeof(aBuf))-1))
	{
		m_pGameConsole->PrintLine(CGameConsole::CONSOLETYPE_ECON, aBuf);

		std::cout << "PASSWORD default" << std::endl;
		m_Connection.Send("default");
		m_State = STATE_AUTHED;
	}
}
