#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

#include <engine/shared/host_lookup.h>
#include <engine/shared/jobs.h>

#include <functional>

static const int TEST_NUM_THREADS = 4;

class Jobs : public ::testing::Test
{
protected:
	CJobPool m_Pool;

	void SetUp() override
	{
		m_Pool.Init(TEST_NUM_THREADS);
	}

	void TearDown() override
	{
		m_Pool.Shutdown();
	}

	void Add(std::shared_ptr<IJob> pJob)
	{
		m_Pool.Add(std::move(pJob));
	}
};

class CJob : public IJob
{
	std::function<void()> m_JobFunction;
	void Run() override { m_JobFunction(); }

public:
	CJob(std::function<void()> &&JobFunction) :
		m_JobFunction(JobFunction) {}

	void Abortable(bool Abortable)
	{
		IJob::Abortable(Abortable);
	}
};

TEST_F(Jobs, Constructor)
{
}

TEST_F(Jobs, Simple)
{
	Add(std::make_shared<CJob>([] {}));
}

TEST_F(Jobs, Wait)
{
	SEMAPHORE sphore;
	sphore_init(&sphore);
	Add(std::make_shared<CJob>([&] { sphore_signal(&sphore); }));
	sphore_wait(&sphore);
	sphore_destroy(&sphore);
}

TEST_F(Jobs, AbortAbortable)
{
	auto pJob = std::make_shared<CJob>([&] {});
	pJob->Abortable(true);
	EXPECT_TRUE(pJob->IsAbortable());
	Add(pJob);
	EXPECT_TRUE(pJob->Abort());
	EXPECT_EQ(pJob->State(), IJob::STATE_ABORTED);
}

TEST_F(Jobs, AbortUnabortable)
{
	auto pJob = std::make_shared<CJob>([&] {});
	pJob->Abortable(false);
	EXPECT_FALSE(pJob->IsAbortable());
	Add(pJob);
	EXPECT_FALSE(pJob->Abort());
	EXPECT_NE(pJob->State(), IJob::STATE_ABORTED);
}

TEST_F(Jobs, LookupHost)
{
	static const char *HOST = "example.com";
	static const int NETTYPE = NETTYPE_ALL;
	auto pJob = std::make_shared<CHostLookup>(HOST, NETTYPE);

	EXPECT_STREQ(pJob->Hostname(), HOST);
	EXPECT_EQ(pJob->Nettype(), NETTYPE);

	Add(pJob);
	while(pJob->State() != IJob::STATE_DONE)
	{
		// yay, busy loop...
		thread_yield();
	}

	EXPECT_STREQ(pJob->Hostname(), HOST);
	EXPECT_EQ(pJob->Nettype(), NETTYPE);
	if(pJob->Result() == 0)
	{
		EXPECT_EQ(pJob->Addr().type & NETTYPE, pJob->Addr().type);
	}
}

TEST_F(Jobs, LookupHostWebsocket)
{
	static const char *HOST = "ws://example.com";
	static const int NETTYPE = NETTYPE_ALL;
	auto pJob = std::make_shared<CHostLookup>(HOST, NETTYPE);

	EXPECT_STREQ(pJob->Hostname(), HOST);
	EXPECT_EQ(pJob->Nettype(), NETTYPE);

	Add(pJob);
	while(pJob->State() != IJob::STATE_DONE)
	{
		// yay, busy loop...
		thread_yield();
	}

	EXPECT_STREQ(pJob->Hostname(), HOST);
	EXPECT_EQ(pJob->Nettype(), NETTYPE);
	if(pJob->Result() == 0)
	{
		EXPECT_EQ(pJob->Addr().type & NETTYPE_WEBSOCKET_IPV4, pJob->Addr().type);
	}
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
		EXPECT_EQ(pJob->State(), IJob::STATE_QUEUED);
		vpJobs.push_back(pJob);
	}
	for(auto &pJob : vpJobs)
	{
		Add(pJob);
	}
	sphore_wait(&sphore);
	sphore_destroy(&sphore);
	TearDown();
	for(auto &pJob : vpJobs)
	{
		EXPECT_EQ(pJob->State(), IJob::STATE_DONE);
	}
	SetUp();
}
