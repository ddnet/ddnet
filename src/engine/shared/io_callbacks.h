#ifndef ENGINE_SHARED_IO_CALLBACKS_H
#define ENGINE_SHARED_IO_CALLBACKS_H

#include <base/types.h>
#include <chrono>

typedef NETSOCKET (*FUdpCreate)(NETADDR BindAddr, void *pUser);
typedef void (*FUdpClose)(NETSOCKET Sock, void *pUser);
typedef int (*FUdpRecv)(NETSOCKET Sock, NETADDR *pAddr, unsigned char **ppData, void *pUser);
typedef int (*FUdpSend)(NETSOCKET Sock, const NETADDR *pAddr, const void *pData, int Size, void *pUser);
typedef int (*FSocketReadWait)(NETSOCKET Sock, std::chrono::nanoseconds Nanoseconds, void *pUser);

class CIoCallbacks
{
	FUdpCreate m_pfnUdpCreate = nullptr;
	FUdpClose m_pfnUdpClose = nullptr;
	FUdpRecv m_pfnUdpRecv = nullptr;
	FUdpSend m_pfngUdpSend = nullptr;
	FSocketReadWait m_pfnSocketReadWait = nullptr;

	void *m_pUser = nullptr;

public:
	void Init();

	NETSOCKET UdpCreate(NETADDR BindAddr);
	void UdpClose(NETSOCKET Sock);
	int UdpRecv(NETSOCKET Sock, NETADDR *pAddr, unsigned char **ppData);
	int UdpSend(NETSOCKET Sock, const NETADDR *pAddr, const void *pData, int Size);
	int SocketReadWait(NETSOCKET Sock, std::chrono::nanoseconds Nanoseconds);
};

namespace IoNetwork {

NETSOCKET net_udp_create(NETADDR BindAddr, void *pUser);
void net_udp_close(NETSOCKET Sock, void *pUser);
int net_udp_recv(NETSOCKET Sock, NETADDR *pAddr, unsigned char **ppData, void *pUser);
int net_udp_send(NETSOCKET Sock, const NETADDR *pAddr, const void *pData, int Size, void *pUser);
int net_socket_read_wait(NETSOCKET Sock, std::chrono::nanoseconds Nanoseconds, void *pUser);

}; // namespace IoNetwork

#endif
