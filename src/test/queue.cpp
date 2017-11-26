#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <base/tl/queue.h>

TEST(Queue, SingleThreadInts)
{
	queue<int> Q;

	for(int i = 0; i < 10; i++)
		Q.Emplace(i);

	for(int i = 0; i < 10; i++)
	{
		EXPECT_EQ(i, *Q.Pop());
	}
}

TEST(Queue, MultiConsumerProducerInts)
{
	queue<int> Q;
	std::vector<std::thread> Threads;
	Threads.reserve(20);

	std::atomic_int s(0);

	for(int i = 0; i < 5; i++)
		Threads.emplace_back([&Q, &s] {
			for(int j = 1; j <= 100; j++)
				s += *Q.Pop();
		});

	for(int i = 0; i < 10; i++)
		Threads.emplace_back([&Q] {
			for(int j = 1; j <= 100; j++)
				Q.Emplace(j);
		});

	for(int i = 0; i < 5; i++)
		Threads.emplace_back([&Q, &s] {
			for(int j = 1; j <= 100; j++)
				s += *Q.Pop();
		});

	for(auto &th : Threads)
		th.join();

	EXPECT_EQ(50500, s);
}

TEST(Queue, QueueStringCopy)
{
	queue<std::string, std::shared_ptr<std::string>> Q1;

	for(int i = 0; i < 10; i++)
		Q1.Emplace("Hello World !");

	Q1.Finish();

	queue<std::string, std::shared_ptr<std::string>> Q2(Q1);

	while(true)
	{
		try
		{
			EXPECT_EQ(*Q1.Pop(), *Q2.Pop());
		}
		catch(QueueDone &)
		{
			break;
		}
	}
}

TEST(Queue, Locking)
{
	std::vector<std::thread> Threads;
	queue<int> Q;

	for(int i = 0; i < 5; i++)
		Threads.emplace_back([&Q] {
			auto Lck = Q.GetLock();
			for(int i = 0; i < 20; i++)
				Q.Emplace(i);
		});

	for(int i = 0; i < 100; i++)
		EXPECT_EQ(*Q.Pop(), i % 20);

	for(auto &th : Threads)
		th.join();
}

