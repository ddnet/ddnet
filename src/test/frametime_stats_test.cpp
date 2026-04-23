#include <engine/client/graph.h>

#include <gtest/gtest.h>

#include <cmath>

TEST(GraphSummaryStats, EmptyWindow)
{
	CGraph Graph(1000, 2, true);

	const auto Stats = Graph.SummaryStats(360);
	EXPECT_FALSE(Stats.IsValid());
	EXPECT_EQ(Stats.m_NumSamples, 0u);
}

TEST(GraphSummaryStats, PartialWindow)
{
	CGraph Graph(1000, 2, true);
	Graph.InsertAt(1, 0.001f);
	Graph.InsertAt(2, 0.003f);
	Graph.InsertAt(3, 0.002f);

	const auto Stats = Graph.SummaryStats(360);
	ASSERT_TRUE(Stats.IsValid());
	EXPECT_EQ(Stats.m_NumSamples, 3u);
	EXPECT_FLOAT_EQ(Stats.m_Min, 0.001f);
	EXPECT_NEAR(Stats.m_Avg, 0.002f, 1e-6f);
	EXPECT_NEAR(Stats.m_Deviation, std::sqrt(2.0f / 3.0f) * 0.001f, 1e-6f);
	EXPECT_FLOAT_EQ(Stats.m_Max, 0.003f);
}

TEST(GraphSummaryStats, Exact360FrameWindow)
{
	CGraph Graph(1000, 2, true);
	for(int i = 1; i <= 360; ++i)
	{
		Graph.InsertAt(i, i / 1000000.0f);
	}

	const auto Stats = Graph.SummaryStats(360);
	ASSERT_TRUE(Stats.IsValid());
	EXPECT_EQ(Stats.m_NumSamples, 360u);
	EXPECT_FLOAT_EQ(Stats.m_Min, 0.000001f);
	EXPECT_FLOAT_EQ(Stats.m_Max, 0.000360f);
	EXPECT_NEAR(Stats.m_Avg, 0.0001805f, 1e-6f);
	EXPECT_NEAR(Stats.m_Deviation, 0.0001039226f, 1e-6f);
}

TEST(GraphSummaryStats, Exact1000FrameWindow)
{
	CGraph Graph(1000, 2, true);
	for(int i = 1; i <= 1000; ++i)
	{
		Graph.InsertAt(i, i / 1000000.0f);
	}

	const auto Stats = Graph.SummaryStats(1000);
	ASSERT_TRUE(Stats.IsValid());
	EXPECT_EQ(Stats.m_NumSamples, 1000u);
	EXPECT_FLOAT_EQ(Stats.m_Min, 0.000001f);
	EXPECT_FLOAT_EQ(Stats.m_Max, 0.001000f);
	EXPECT_NEAR(Stats.m_Avg, 0.0005005f, 1e-6f);
	EXPECT_NEAR(Stats.m_Deviation, 0.00028867499f, 1e-6f);
}

TEST(GraphSummaryStats, RolloverKeepsLatestSamples)
{
	CGraph Graph(1000, 2, true);
	for(int i = 1; i <= 1200; ++i)
	{
		Graph.InsertAt(i, i / 1000000.0f);
	}

	const auto Stats360 = Graph.SummaryStats(360);
	ASSERT_TRUE(Stats360.IsValid());
	EXPECT_EQ(Stats360.m_NumSamples, 360u);
	EXPECT_FLOAT_EQ(Stats360.m_Min, 0.000841f);
	EXPECT_FLOAT_EQ(Stats360.m_Max, 0.001200f);
	EXPECT_NEAR(Stats360.m_Avg, 0.0010205f, 1e-6f);
	EXPECT_NEAR(Stats360.m_Deviation, 0.0001039226f, 1e-6f);

	const auto Stats1000 = Graph.SummaryStats(1000);
	ASSERT_TRUE(Stats1000.IsValid());
	EXPECT_EQ(Stats1000.m_NumSamples, 1000u);
	EXPECT_FLOAT_EQ(Stats1000.m_Min, 0.000201f);
	EXPECT_FLOAT_EQ(Stats1000.m_Max, 0.001200f);
	EXPECT_NEAR(Stats1000.m_Avg, 0.0007005f, 1e-6f);
	EXPECT_NEAR(Stats1000.m_Deviation, 0.00028867499f, 1e-6f);
}

TEST(GraphSummaryStats, OldSpikeAgesOut)
{
	CGraph Graph(1000, 2, true);
	Graph.InsertAt(1, 0.100f);
	for(int i = 0; i < 999; ++i)
	{
		Graph.InsertAt(i + 2, 0.001f);
	}

	const auto StatsWithSpike = Graph.SummaryStats(1000);
	ASSERT_TRUE(StatsWithSpike.IsValid());
	EXPECT_FLOAT_EQ(StatsWithSpike.m_Min, 0.001f);
	EXPECT_FLOAT_EQ(StatsWithSpike.m_Max, 0.100f);
	EXPECT_NEAR(StatsWithSpike.m_Avg, 0.001099f, 1e-6f);
	EXPECT_NEAR(StatsWithSpike.m_Deviation, 0.0031299679f, 1e-6f);

	Graph.InsertAt(1001, 0.002f);

	const auto StatsWithoutSpike = Graph.SummaryStats(1000);
	ASSERT_TRUE(StatsWithoutSpike.IsValid());
	EXPECT_FLOAT_EQ(StatsWithoutSpike.m_Min, 0.001f);
	EXPECT_FLOAT_EQ(StatsWithoutSpike.m_Max, 0.002f);
	EXPECT_NEAR(StatsWithoutSpike.m_Avg, 0.001001f, 1e-6f);
	EXPECT_NEAR(StatsWithoutSpike.m_Deviation, 0.0000316070f, 1e-6f);
}
