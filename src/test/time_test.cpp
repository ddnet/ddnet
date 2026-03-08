#include <base/time.h>

#include <gtest/gtest.h>

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
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	const std::chrono::nanoseconds Time2 = time_get_nanoseconds();
	EXPECT_LT(Time1, Time2);
}

TEST(Time, StrTime)
{
	char aBuf[32] = "foobar";

	EXPECT_EQ(str_time(-123456, ETimeFormat::MINS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "00.00");

	EXPECT_EQ(str_time(INT64_MAX, ETimeFormat::DAYS, aBuf, sizeof(aBuf)), 23);
	EXPECT_STREQ(aBuf, "1067519911673d 00:09:18");

	EXPECT_EQ(str_time(123456, ETimeFormat::DAYS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "20:34");
	EXPECT_EQ(str_time(1234567, ETimeFormat::DAYS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "03:25:45");
	EXPECT_EQ(str_time(12345678, ETimeFormat::DAYS, aBuf, sizeof(aBuf)), 11);
	EXPECT_STREQ(aBuf, "1d 10:17:36");

	EXPECT_EQ(str_time(123456, ETimeFormat::HOURS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "20:34");
	EXPECT_EQ(str_time(1234567, ETimeFormat::HOURS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "03:25:45");
	EXPECT_EQ(str_time(12345678, ETimeFormat::HOURS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "34:17:36");

	EXPECT_EQ(str_time(123456, ETimeFormat::MINS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "20:34");
	EXPECT_EQ(str_time(1234567, ETimeFormat::MINS, aBuf, sizeof(aBuf)), 6);
	EXPECT_STREQ(aBuf, "205:45");
	EXPECT_EQ(str_time(12345678, ETimeFormat::MINS, aBuf, sizeof(aBuf)), 7);
	EXPECT_STREQ(aBuf, "2057:36");

	EXPECT_EQ(str_time(123456, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "20:34.56");
	EXPECT_EQ(str_time(1234567, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf)), 11);
	EXPECT_STREQ(aBuf, "03:25:45.67");
	EXPECT_EQ(str_time(12345678, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf)), 11);
	EXPECT_STREQ(aBuf, "34:17:36.78");

	EXPECT_EQ(str_time(123456, ETimeFormat::MINS_CENTISECS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "20:34.56");
	EXPECT_EQ(str_time(1234567, ETimeFormat::MINS_CENTISECS, aBuf, sizeof(aBuf)), 9);
	EXPECT_STREQ(aBuf, "205:45.67");
	EXPECT_EQ(str_time(12345678, ETimeFormat::MINS_CENTISECS, aBuf, sizeof(aBuf)), 10);
	EXPECT_STREQ(aBuf, "2057:36.78");

	EXPECT_EQ(str_time(123456, ETimeFormat::SECS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "34.56");
	EXPECT_EQ(str_time(1234567, ETimeFormat::SECS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "45.67");
	EXPECT_EQ(str_time(12345678, ETimeFormat::SECS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "36.78");
}

TEST(Time, MillisecondsFromSeconds)
{
	EXPECT_EQ(time_milliseconds_from_seconds(-0.001f), -1);
	EXPECT_EQ(time_milliseconds_from_seconds(0.0f), 0);
	EXPECT_EQ(time_milliseconds_from_seconds(0.0001f), 0);
	EXPECT_EQ(time_milliseconds_from_seconds(0.0006f), 1);
	EXPECT_EQ(time_milliseconds_from_seconds(0.001f), 1);
	EXPECT_EQ(time_milliseconds_from_seconds(1.000f), 1000);
	EXPECT_EQ(time_milliseconds_from_seconds(36000000.000f), 36000000000);
}

TEST(Time, StrTimeFloat)
{
	char aBuf[64];
	EXPECT_EQ(str_time_float(123456.78, ETimeFormat::DAYS, aBuf, sizeof(aBuf)), 11);
	EXPECT_STREQ(aBuf, "1d 10:17:36");

	EXPECT_EQ(str_time_float(12.16, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "12.16");

	EXPECT_EQ(str_time_float(22.995, ETimeFormat::MINS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "00:22");

	EXPECT_EQ(str_time_float(36000000.0f, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf)), 14);
	EXPECT_STREQ(aBuf, "10000:00:00.00");
}
