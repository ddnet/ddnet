#include <gtest/gtest.h>

#include <base/system.h>

#include <thread>

TEST(Time, Timestamp)
{
	(void)time_timestamp();
}

TEST(Time, HourOfTheDay)
{
	const int Hour = time_houroftheday();
	EXPECT_TRUE(Hour >= 0 && Hour <= 23);
}

TEST(Time, Season)
{
	(void)time_season();
}

TEST(Time, Nanoseconds)
{
	const std::chrono::nanoseconds Time1 = time_get_nanoseconds();
	std::this_thread::sleep_for(std::chrono::microseconds(1));
	const std::chrono::nanoseconds Time2 = time_get_nanoseconds();
	EXPECT_LT(Time1, Time2);
}
