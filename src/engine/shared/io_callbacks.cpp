#include <base/system.h>
#include <base/types.h>

#include "io_callbacks.h"

void CIoCallbacks::Init()
{
	m_pfnUdpCreate = IoNetwork::net_udp_create;
	m_pfnUdpClose = IoNetwork::net_udp_close;
	m_pfnUdpRecv = IoNetwork::net_udp_recv;
	m_pfngUdpSend = IoNetwork::net_udp_send;
	m_pfnSocketReadWait = IoNetwork::net_socket_read_wait;

	m_pUser = nullptr;
}

NETSOCKET CIoCallbacks::UdpCreate(NETADDR BindAddr)
{
	return m_pfnUdpCreate(BindAddr, m_pUser);
}

void CIoCallbacks::UdpClose(NETSOCKET Sock)
{
	m_pfnUdpClose(Sock, m_pUser);
}

int CIoCallbacks::UdpRecv(NETSOCKET Sock, NETADDR *pAddr, unsigned char **ppData)
{
	return m_pfnUdpRecv(Sock, pAddr, ppData, m_pUser);
}

int CIoCallbacks::UdpSend(NETSOCKET Sock, const NETADDR *pAddr, const void *pData, int Size)
{
	return m_pfngUdpSend(Sock, pAddr, pData, Size, m_pUser);
}

int CIoCallbacks::SocketReadWait(NETSOCKET Sock, std::chrono::nanoseconds Nanoseconds)
{
	return m_pfnSocketReadWait(Sock, Nanoseconds, m_pUser);
}

namespace IoNetwork {

NETSOCKET net_udp_create(NETADDR BindAddr, void *pUser)
{
	return ::net_udp_create(BindAddr);
}

void net_udp_close(NETSOCKET Sock, void *pUser)
{
	::net_udp_close(Sock);
}

int net_udp_recv(NETSOCKET Sock, NETADDR *pAddr, unsigned char **ppData, void *pUser)
{
	return ::net_udp_recv(Sock, pAddr, ppData);
}

int net_udp_send(NETSOCKET Sock, const NETADDR *pAddr, const void *pData, int Size, void *pUser)
{
	return ::net_udp_send(Sock, pAddr, pData, Size);
}

int net_socket_read_wait(NETSOCKET Sock, std::chrono::nanoseconds Nanoseconds, void *pUser)
{
	return ::net_socket_read_wait(Sock, Nanoseconds);
}

}; // namespace IoNetwork
