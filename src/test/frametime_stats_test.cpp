#include <engine/client.h>

#include <gtest/gtest.h>

#include <cmath>

TEST(FrameTimeHistory, EmptyWindow)
{
	CFrameTimeHistory History;

	const auto Stats = History.GetStats(360);
	EXPECT_FALSE(Stats.IsValid());
	EXPECT_EQ(Stats.m_NumSamples, 0u);
}

TEST(FrameTimeHistory, PartialWindow)
{
	CFrameTimeHistory History;
	History.Add(0.001f);
	History.Add(0.003f);
	History.Add(0.002f);

	const auto Stats = History.GetStats(360);
	ASSERT_TRUE(Stats.IsValid());
	EXPECT_EQ(Stats.m_NumSamples, 3u);
	EXPECT_FLOAT_EQ(Stats.m_Min, 0.001f);
	EXPECT_NEAR(Stats.m_Avg, 0.002f, 1e-6f);
	EXPECT_NEAR(Stats.m_Deviation, std::sqrt(2.0f / 3.0f) * 0.001f, 1e-6f);
	EXPECT_FLOAT_EQ(Stats.m_Max, 0.003f);
}

TEST(FrameTimeHistory, Exact360FrameWindow)
{
	CFrameTimeHistory History;
	for(int i = 1; i <= 360; ++i)
	{
		History.Add(i / 1000000.0f);
	}

	const auto Stats = History.GetStats(360);
	ASSERT_TRUE(Stats.IsValid());
	EXPECT_EQ(Stats.m_NumSamples, 360u);
	EXPECT_FLOAT_EQ(Stats.m_Min, 0.000001f);
	EXPECT_FLOAT_EQ(Stats.m_Max, 0.000360f);
	EXPECT_NEAR(Stats.m_Avg, 0.0001805f, 1e-6f);
	EXPECT_NEAR(Stats.m_Deviation, 0.0001039226f, 1e-6f);
}

TEST(FrameTimeHistory, Exact1000FrameWindow)
{
	CFrameTimeHistory History;
	for(int i = 1; i <= 1000; ++i)
	{
		History.Add(i / 1000000.0f);
	}

	const auto Stats = History.GetStats(1000);
	ASSERT_TRUE(Stats.IsValid());
	EXPECT_EQ(Stats.m_NumSamples, 1000u);
	EXPECT_FLOAT_EQ(Stats.m_Min, 0.000001f);
	EXPECT_FLOAT_EQ(Stats.m_Max, 0.001000f);
	EXPECT_NEAR(Stats.m_Avg, 0.0005005f, 1e-6f);
	EXPECT_NEAR(Stats.m_Deviation, 0.00028867499f, 1e-6f);
}

TEST(FrameTimeHistory, RolloverKeepsLatestSamples)
{
	CFrameTimeHistory History;
	for(int i = 1; i <= 1200; ++i)
	{
		History.Add(i / 1000000.0f);
	}

	const auto Stats360 = History.GetStats(360);
	ASSERT_TRUE(Stats360.IsValid());
	EXPECT_EQ(Stats360.m_NumSamples, 360u);
	EXPECT_FLOAT_EQ(Stats360.m_Min, 0.000841f);
	EXPECT_FLOAT_EQ(Stats360.m_Max, 0.001200f);
	EXPECT_NEAR(Stats360.m_Avg, 0.0010205f, 1e-6f);
	EXPECT_NEAR(Stats360.m_Deviation, 0.0001039226f, 1e-6f);

	const auto Stats1000 = History.GetStats(1000);
	ASSERT_TRUE(Stats1000.IsValid());
	EXPECT_EQ(Stats1000.m_NumSamples, 1000u);
	EXPECT_FLOAT_EQ(Stats1000.m_Min, 0.000201f);
	EXPECT_FLOAT_EQ(Stats1000.m_Max, 0.001200f);
	EXPECT_NEAR(Stats1000.m_Avg, 0.0007005f, 1e-6f);
	EXPECT_NEAR(Stats1000.m_Deviation, 0.00028867499f, 1e-6f);
}

TEST(FrameTimeHistory, OldSpikeAgesOut)
{
	CFrameTimeHistory History;
	History.Add(0.100f);
	for(int i = 0; i < 999; ++i)
	{
		History.Add(0.001f);
	}

	const auto StatsWithSpike = History.GetStats(1000);
	ASSERT_TRUE(StatsWithSpike.IsValid());
	EXPECT_FLOAT_EQ(StatsWithSpike.m_Min, 0.001f);
	EXPECT_FLOAT_EQ(StatsWithSpike.m_Max, 0.100f);
	EXPECT_NEAR(StatsWithSpike.m_Avg, 0.001099f, 1e-6f);
	EXPECT_NEAR(StatsWithSpike.m_Deviation, 0.0031299679f, 1e-6f);

	History.Add(0.002f);

	const auto StatsWithoutSpike = History.GetStats(1000);
	ASSERT_TRUE(StatsWithoutSpike.IsValid());
	EXPECT_FLOAT_EQ(StatsWithoutSpike.m_Min, 0.001f);
	EXPECT_FLOAT_EQ(StatsWithoutSpike.m_Max, 0.002f);
	EXPECT_NEAR(StatsWithoutSpike.m_Avg, 0.001001f, 1e-6f);
	EXPECT_NEAR(StatsWithoutSpike.m_Deviation, 0.0000316070f, 1e-6f);
}
