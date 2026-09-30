#include "test.h"

#include <base/io.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/secure.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client/serverbrowser_http.h>
#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/engine.h>
#include <engine/http.h>
#include <engine/storage.h>

#include <game/version.h>

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

// A minimal server list that the parser in serverbrowser_http.cpp accepts.
static const char *SMALL_SERVERLIST = R"({"servers":[{"addresses":["tw-0.6+udp://127.0.0.1:8303"],"info":{"max_clients":16,"max_players":16,"passworded":false,"client_score_kind":"points","game_type":"DDNet","name":"test","map":{"name":"dm1"},"version":"1","clients":[]}}]})";

// Counts the requests a server browser sends to a master server, so that the
// request amplification of a browser refresh can be measured.
class CFakeMaster
{
public:
	CFakeMaster();
	~CFakeMaster();

	int Port() const { return m_Port; }

	// Requests that the master answered with a complete response.
	int CompletedRequests() const { return m_CompletedRequests.load(); }
	int64_t BodyBytesSent() const { return m_BodyBytesSent.load(); }

	void SetListBody(std::string Body) { m_ListBody = std::move(Body); }

private:
	void Run();

	NETSOCKET m_Socket = nullptr;
	int m_Port = 0;
	std::thread m_Thread;
	std::atomic<bool> m_Stop{false};
	std::atomic<int> m_CompletedRequests{0};
	std::atomic<int64_t> m_BodyBytesSent{0};
	std::string m_ListBody = SMALL_SERVERLIST;
};

CFakeMaster::CFakeMaster()
{
	// Bind a port in the private range. There is no portable way to ask the OS
	// for an ephemeral port through base/net.h, so probe a few instead.
	secure_random_fill(&m_Port, sizeof(m_Port));
	m_Port = 40000 + (m_Port % 10000);
	for(int Attempt = 0; Attempt < 100; Attempt++, m_Port++)
	{
		NETADDR BindAddr;
		mem_zero(&BindAddr, sizeof(BindAddr));
		BindAddr.type = NETTYPE_IPV4;
		BindAddr.ip[0] = 127;
		BindAddr.ip[3] = 1;
		BindAddr.port = m_Port;
		m_Socket = net_tcp_create(BindAddr);
		if(m_Socket && net_tcp_listen(m_Socket, 16) == 0)
		{
			break;
		}
		net_tcp_close(m_Socket);
		m_Socket = nullptr;
	}
	if(!m_Socket)
	{
		return;
	}
	net_set_non_blocking(m_Socket);
	m_Thread = std::thread([this]() { Run(); });
}

CFakeMaster::~CFakeMaster()
{
	m_Stop.store(true);
	if(m_Thread.joinable())
	{
		m_Thread.join();
	}
	if(m_Socket)
	{
		net_tcp_close(m_Socket);
		m_Socket = nullptr;
	}
}

void CFakeMaster::Run()
{
	while(!m_Stop.load())
	{
		net_socket_read_wait(m_Socket, std::chrono::milliseconds(10));
		NETSOCKET Socket;
		NETADDR Addr;
		if(net_tcp_accept(m_Socket, &Socket, &Addr) <= 0)
		{
			continue;
		}

		std::string aRequest;
		char aBuf[1024];
		while(aRequest.find("\r\n\r\n") == std::string::npos)
		{
			const int Read = net_tcp_recv(Socket, aBuf, sizeof(aBuf));
			if(Read <= 0)
			{
				break;
			}
			aRequest.append(aBuf, Read);
		}

		const bool IsHead = aRequest.compare(0, 5, "HEAD ") == 0;
		char aHeader[256];
		str_format(aHeader, sizeof(aHeader),
			"HTTP/1.1 200 OK\r\nContent-Length: %zu\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n",
			m_ListBody.size());
		int64_t Sent = net_tcp_send(Socket, aHeader, str_length(aHeader));
		if(!IsHead)
		{
			// Send in chunks so that a client which stops reading is noticed
			// instead of the whole body landing in the socket buffer.
			for(size_t Offset = 0; Offset < m_ListBody.size(); Offset += 64 * 1024)
			{
				const int Chunk = (int)std::min<size_t>(64 * 1024, m_ListBody.size() - Offset);
				const int Written = net_tcp_send(Socket, m_ListBody.data() + Offset, Chunk);
				if(Written <= 0)
				{
					break;
				}
				Sent += Written;
			}
		}
		net_tcp_close(Socket);

		if(Sent == (int64_t)str_length(aHeader) + (int64_t)(IsHead ? 0 : m_ListBody.size()))
		{
			m_CompletedRequests.fetch_add(1);
		}
		if(!IsHead)
		{
			m_BodyBytesSent.fetch_add(Sent);
		}
	}
}

class CServerBrowserHttpTest : public ::testing::Test
{
public:
	void SetUp() override
	{
		ASSERT_NE(m_Master.Port(), 0) << "failed to start fake master server";
	}

	void SetUpBrowser(CFakeMaster *pMaster)
	{
		m_pStorage = m_TestInfo.CreateTestStorage();
		ASSERT_NE(m_pStorage, nullptr);
		char aUrlLine[128];
		str_format(aUrlLine, sizeof(aUrlLine), "http://127.0.0.1:%d/ddnet/15/servers.json\n", pMaster->Port());
		IOHANDLE File = m_pStorage->OpenFile("ddnet-serverlist-urls.cfg", IOFLAG_WRITE, IStorage::TYPE_SAVE);
		ASSERT_NE(File, nullptr);
		io_write(File, aUrlLine, str_length(aUrlLine));
		io_close(File);

		m_pConsole.reset(CreateConsole(CFGFLAG_CLIENT).release());
		// The fake master is plain HTTP.
		g_Config.m_HttpAllowInsecure = 1;
		m_pEngine.reset(CreateTestEngine(GAME_NAME));
		m_pEngineHttp.reset(CreateEngineHttp());
		ASSERT_TRUE(m_pEngineHttp->Init(std::chrono::seconds{2})) << "failed to initialize the HTTP client";
	}

	void TearDown() override
	{
		m_pBrowser.reset();
		if(m_pEngineHttp)
		{
			m_pEngineHttp->Shutdown();
		}
		m_pEngine.reset();
		m_pEngineHttp.reset();
		m_pConsole.reset();
		m_pStorage.reset();
	}

	IServerBrowserHttp *CreateBrowser()
	{
		m_pBrowser = std::unique_ptr<IServerBrowserHttp>(CreateServerBrowserHttp(m_pEngine.get(), m_pStorage.get(), m_pEngineHttp.get(), ""));
		return m_pBrowser.get();
	}

	// Runs the browser until it is no longer fetching a server list.
	void PumpUntilDone(IServerBrowserHttp *pBrowser)
	{
		const int64_t Deadline = time_get() + 20 * time_freq();
		while(pBrowser->IsRefreshing() && time_get() < Deadline)
		{
			pBrowser->Update();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		pBrowser->Update();
	}

	CTestInfo m_TestInfo;
	CFakeMaster m_Master;
	std::unique_ptr<IStorage> m_pStorage;
	std::unique_ptr<IConsole> m_pConsole;
	std::unique_ptr<IEngine> m_pEngine;
	std::unique_ptr<IEngineHttp> m_pEngineHttp;
	std::unique_ptr<IServerBrowserHttp> m_pBrowser;
};

// Option 1: the refresh button has no rate limit, so every press asks the
// master for a fresh list. One press costs a HEAD and a GET for master
// selection plus a GET for the list itself.
TEST_F(CServerBrowserHttpTest, RefreshRequestFlood)
{
	SetUpBrowser(&m_Master);
	IServerBrowserHttp *pBrowser = CreateBrowser();
	ASSERT_TRUE(pBrowser != nullptr);
	PumpUntilDone(pBrowser);
	const int Baseline = m_Master.CompletedRequests();
	EXPECT_GT(Baseline, 0);

	const int Refreshes = 5;
	for(int i = 0; i < Refreshes; i++)
	{
		pBrowser->Refresh();
		PumpUntilDone(pBrowser);
	}

	const int Requests = m_Master.CompletedRequests() - Baseline;
	// Without a rate limit this is 3 per press. The master must not be able to
	// be made to serve the full list 3 * Refreshes times.
	EXPECT_LE(Requests, Refreshes);
}

// Option 2: the server list response is read without a size limit, so a
// master can make the client buffer an arbitrarily large body.
TEST_F(CServerBrowserHttpTest, ServerListSizeLimit)
{
	// 48 MiB of server list, far more than any real one. The padding keeps the
	// response cheap to parse so that this test only measures the download.
	const size_t HugeSize = 48 * 1024 * 1024;
	std::string HugeList = "{\"servers\":[],\"padding\":\"";
	HugeList.append(HugeSize, 'a');
	HugeList += "\"}";
	ASSERT_GT(HugeList.size(), HugeSize);
	m_Master.SetListBody(std::move(HugeList));

	SetUpBrowser(&m_Master);
	IServerBrowserHttp *pBrowser = CreateBrowser();
	ASSERT_TRUE(pBrowser != nullptr);
	PumpUntilDone(pBrowser);
	pBrowser->Refresh();
	PumpUntilDone(pBrowser);

	// The client must not buffer the whole oversized body.
	EXPECT_LT(m_Master.BodyBytesSent(), static_cast<int64_t>(HugeSize));
}
