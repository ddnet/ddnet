#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/engine.h>
#include <engine/shared/jobs.h>

#include <functional>

static const int TEST_NUM_THREADS = 4;

class Jobs : public ::testing::Test
{
protected:
	CJobPool m_Pool;

	Jobs()
	{
		m_Pool.Init(TEST_NUM_THREADS);
	}

	void Add(std::shared_ptr<IJob> pJob)
	{
		m_Pool.Add(std::move(pJob));
	}
	void RunBlocking(IJob *pJob)
	{
		CJobPool::RunBlocking(pJob);
	}
};

class CJob : public IJob
{
	std::function<void()> m_JobFunction;
	void Run() override { m_JobFunction(); }

public:
	CJob(std::function<void()> &&JobFunction) :
		m_JobFunction(JobFunction) {}
};

TEST_F(Jobs, Constructor)
{
}

TEST_F(Jobs, Simple)
{
	Add(std::make_shared<CJob>([] {}));
}

TEST_F(Jobs, RunBlocking)
{
	int Result = 0;
	CJob Job([&] { Result = 1; });
	EXPECT_EQ(Result, 0);
	RunBlocking(&Job);
	EXPECT_EQ(Result, 1);
}

TEST_F(Jobs, Wait)
{
	SEMAPHORE sphore;
	sphore_init(&sphore);
	Add(std::make_shared<CJob>([&] { sphore_signal(&sphore); }));
	sphore_wait(&sphore);
	sphore_destroy(&sphore);
}

TEST_F(Jobs, LookupHost)
{
	static const char *HOST = "example.com";
	static const int NETTYPE = NETTYPE_ALL;
	auto pJob = std::make_shared<CHostLookup>(HOST, NETTYPE);

	EXPECT_STREQ(pJob->m_aHostname, HOST);
	EXPECT_EQ(pJob->m_Nettype, NETTYPE);

	Add(pJob);
	while(pJob->Status() != IJob::STATE_DONE)
	{
		// yay, busy loop...
		thread_yield();
	}

	EXPECT_STREQ(pJob->m_aHostname, HOST);
	EXPECT_EQ(pJob->m_Nettype, NETTYPE);
	ASSERT_EQ(pJob->m_Result, 0);
	EXPECT_EQ(pJob->m_Addr.type & NETTYPE, pJob->m_Addr.type);
}

TEST_F(Jobs, LookupHostWebsocket)
{
	static const char *HOST = "ws://example.com";
	static const int NETTYPE = NETTYPE_ALL;
	auto pJob = std::make_shared<CHostLookup>(HOST, NETTYPE);

	EXPECT_STREQ(pJob->m_aHostname, HOST);
	EXPECT_EQ(pJob->m_Nettype, NETTYPE);

	Add(pJob);
	while(pJob->Status() != IJob::STATE_DONE)
	{
		// yay, busy loop...
		thread_yield();
	}

	EXPECT_STREQ(pJob->m_aHostname, HOST);
	EXPECT_EQ(pJob->m_Nettype, NETTYPE);
	ASSERT_EQ(pJob->m_Result, 0);
	EXPECT_EQ(pJob->m_Addr.type & NETTYPE_WEBSOCKET_IPV4, pJob->m_Addr.type);
}

TEST_F(Jobs, Many)
{
	std::atomic<int> ThreadsRunning(0);
	std::vector<std::shared_ptr<IJob>> vpJobs;
	SEMAPHORE sphore;
	sphore_init(&sphore);
	for(int i = 0; i < TEST_NUM_THREADS; i++)
	{
		std::shared_ptr<IJob> pJob = std::make_shared<CJob>([&] {
			int Prev = ThreadsRunning.fetch_add(1);
			if(Prev == TEST_NUM_THREADS - 1)
			{
				sphore_signal(&sphore);
			}
		});
		EXPECT_EQ(pJob->Status(), IJob::STATE_PENDING);
		vpJobs.push_back(pJob);
	}
	for(auto &pJob : vpJobs)
	{
		Add(pJob);
	}
	sphore_wait(&sphore);
	sphore_destroy(&sphore);
	m_Pool.~CJobPool();
	for(auto &pJob : vpJobs)
	{
		EXPECT_EQ(pJob->Status(), IJob::STATE_DONE);
	}
	new(&m_Pool) CJobPool();
}
