#include <base/mem.h>
#include <base/net.h>
#include <base/secure.h>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

TEST(Net, Ipv4AndIpv6Work)
{
	NETADDR Bindaddr = {};
	NETSOCKET Socket1;
	NETSOCKET Socket2;

	Bindaddr.type = NETTYPE_IPV4 | NETTYPE_IPV6;
	Socket2 = net_udp_create(Bindaddr);
	do
	{
		Bindaddr.port = secure_rand_below(65535 - 1024) + 1024;
	} while(!(Socket1 = net_udp_create(Bindaddr)));

	NETADDR LocalhostV4;
	NETADDR LocalhostV6;
	NETADDR TargetV4;
	NETADDR TargetV6;
	ASSERT_FALSE(net_addr_from_str(&LocalhostV4, "127.0.0.1"));
	ASSERT_FALSE(net_addr_from_str(&LocalhostV6, "[::1]"));
	TargetV4 = LocalhostV4;
	TargetV6 = LocalhostV6;
	TargetV4.port = Bindaddr.port;
	TargetV6.port = Bindaddr.port;

	NETADDR Addr;
	unsigned char *pData;

	EXPECT_EQ(net_udp_send(Socket2, &TargetV4, "abc", 3), 3);

	EXPECT_EQ(net_socket_read_wait(Socket1, 10s), 1);
	ASSERT_EQ(net_udp_recv(Socket1, &Addr, &pData), 3);
	Addr.port = 0;
	EXPECT_EQ(Addr, LocalhostV4);
	EXPECT_EQ(mem_comp(pData, "abc", 3), 0);

	EXPECT_EQ(net_udp_send(Socket2, &TargetV6, "def", 3), 3);

	EXPECT_EQ(net_socket_read_wait(Socket1, 10s), 1);
	ASSERT_EQ(net_udp_recv(Socket1, &Addr, &pData), 3);
	Addr.port = 0;
	EXPECT_EQ(Addr, LocalhostV6);
	EXPECT_EQ(mem_comp(pData, "def", 3), 0);

	net_udp_close(Socket1);
	net_udp_close(Socket2);
}

TEST(Net, Loopback)
{
	net_loopback_set_enabled(true);

	NETADDR Bindaddr = {};
	Bindaddr.type = NETTYPE_IPV4 | NETTYPE_IPV6;
	NETSOCKET ClientSocket = net_udp_create(Bindaddr);
	NETSOCKET ServerSocket;
	do
	{
		Bindaddr.port = secure_rand_below(65535 - 1024) + 1024;
	} while(!(ServerSocket = net_udp_create(Bindaddr)));

	NETADDR TargetV4;
	ASSERT_FALSE(net_addr_from_str(&TargetV4, "127.0.0.1"));
	TargetV4.port = Bindaddr.port;

	NETADDR Addr;
	unsigned char *pData;

	// Client to server, reply from server to client using the sender address.
	EXPECT_EQ(net_udp_send(ClientSocket, &TargetV4, "abc", 3), 3);
	EXPECT_EQ(net_socket_read_wait(ServerSocket, 10s), 1);
	ASSERT_EQ(net_udp_recv(ServerSocket, &Addr, &pData), 3);
	EXPECT_EQ(mem_comp(pData, "abc", 3), 0);
	EXPECT_EQ(Addr.type, NETTYPE_IPV4);
	EXPECT_EQ(Addr.ip[0], 127);

	EXPECT_EQ(net_udp_send(ServerSocket, &Addr, "def", 3), 3);
	EXPECT_EQ(net_socket_read_wait(ClientSocket, 10s), 1);
	ASSERT_EQ(net_udp_recv(ClientSocket, &Addr, &pData), 3);
	EXPECT_EQ(mem_comp(pData, "def", 3), 0);
	// The sender address must match the address the client sent to, so that
	// connection peer address checks are consistent.
	EXPECT_EQ(Addr, TargetV4);

	// The sender address is fabricated in the family of the destination address.
	NETADDR TargetV6;
	ASSERT_FALSE(net_addr_from_str(&TargetV6, "[::1]"));
	TargetV6.port = Bindaddr.port;
	EXPECT_EQ(net_udp_send(ClientSocket, &TargetV6, "ghi", 3), 3);
	EXPECT_EQ(net_socket_read_wait(ServerSocket, 10s), 1);
	ASSERT_EQ(net_udp_recv(ServerSocket, &Addr, &pData), 3);
	EXPECT_EQ(mem_comp(pData, "ghi", 3), 0);
	EXPECT_EQ(Addr.type, NETTYPE_IPV6);
	EXPECT_EQ(Addr.ip[15], 1);

	// A waiting socket is woken up by a packet from another thread.
	std::thread SendThread([&] {
		std::this_thread::sleep_for(50ms);
		net_udp_send(ClientSocket, &TargetV4, "jkl", 3);
	});
	const auto Start = std::chrono::steady_clock::now();
	EXPECT_EQ(net_socket_read_wait(ServerSocket, 10s), 1);
	EXPECT_LT(std::chrono::steady_clock::now() - Start, 5s);
	SendThread.join();
	ASSERT_EQ(net_udp_recv(ServerSocket, &Addr, &pData), 3);
	EXPECT_EQ(mem_comp(pData, "jkl", 3), 0);

	// Sending to a port without an in-process socket goes to the real socket
	// instead of the loopback transport.
	NETADDR Unbound = TargetV4;
	Unbound.port = 1;
	EXPECT_EQ(net_udp_send(ClientSocket, &Unbound, "mno", 3), 3);

	net_udp_close(ServerSocket);
	// After closing, its port has no in-process socket anymore either.
	EXPECT_EQ(net_udp_send(ClientSocket, &TargetV4, "pqr", 3), 3);
	net_udp_close(ClientSocket);

	net_loopback_set_enabled(false);
}

TEST(Net, LoopbackDoesNotSwallowRealLocalhostTraffic)
{
	// A real socket without a loopback registration, like a server running in
	// another process on the same machine.
	NETADDR Bindaddr = {};
	Bindaddr.type = NETTYPE_IPV4;
	NETSOCKET RealSocket;
	do
	{
		Bindaddr.port = secure_rand_below(65535 - 1024) + 1024;
	} while(!(RealSocket = net_udp_create(Bindaddr)));

	net_loopback_set_enabled(true);
	NETADDR ClientBindaddr = {};
	ClientBindaddr.type = NETTYPE_IPV4;
	NETSOCKET ClientSocket = net_udp_create(ClientBindaddr);

	// No in-process socket is bound on the target port, so the packet must
	// reach the real socket instead of being dropped by the loopback transport.
	NETADDR Target;
	ASSERT_FALSE(net_addr_from_str(&Target, "127.0.0.1"));
	Target.port = Bindaddr.port;
	EXPECT_EQ(net_udp_send(ClientSocket, &Target, "abc", 3), 3);
	EXPECT_EQ(net_socket_read_wait(RealSocket, 10s), 1);
	NETADDR Addr;
	unsigned char *pData;
	ASSERT_EQ(net_udp_recv(RealSocket, &Addr, &pData), 3);
	EXPECT_EQ(mem_comp(pData, "abc", 3), 0);

	net_udp_close(ClientSocket);
	net_udp_close(RealSocket);
	net_loopback_set_enabled(false);
}
