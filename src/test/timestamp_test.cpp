#include <base/detect.h>
#include <base/time.h>

#include <gtest/gtest.h>

#include <cstdlib>

class TimestampTest : public testing::Test // NOLINT(readability-identifier-naming)
{
protected:
	static void SetTimezone(const char *pTimezone)
	{
#if defined(CONF_FAMILY_WINDOWS)
		_putenv_s("TZ", pTimezone);
		_tzset();
#else
		setenv("TZ", pTimezone, 1);
		tzset();
#endif
	}

	void SetUp() override
	{
		SetTimezone("UTC");
#if defined(CONF_FAMILY_WINDOWS)
		_dstbias = 0;
#endif
	}

	void TearDown() override
	{
		SetTimezone("UTC");
#if defined(CONF_FAMILY_WINDOWS)
		_dstbias = 0;
#endif
	}
};

TEST_F(TimestampTest, FromStr)
{
	time_t Timestamp;
	EXPECT_TRUE(timestamp_from_str("2023-12-31_12-58-55", TimestampFormat::NOSPACE, &Timestamp));
	EXPECT_EQ(Timestamp, 1704027535);

	EXPECT_TRUE(timestamp_from_str("2012-02-29_13-00-00", TimestampFormat::NOSPACE, &Timestamp));
	EXPECT_EQ(Timestamp, 1330520400);

	EXPECT_TRUE(timestamp_from_str("2004-05-15 18:13:53", TimestampFormat::SPACE, &Timestamp));
	EXPECT_EQ(Timestamp, 1084644833);
}

TEST_F(TimestampTest, FromStrDaylightSavingTime)
{
	SetTimezone("CET-1CEST,M3.5.0,M10.5.0/3");
#if defined(CONF_FAMILY_WINDOWS)
	// Windows is annoying, workaround for test
	_dstbias = -3600;
#endif

	// Format and parse must round trip during daylight saving time
	char aTimestamp[20];
	str_timestamp_ex(1690000000, aTimestamp, sizeof(aTimestamp), TimestampFormat::NOSPACE);
	EXPECT_STREQ(aTimestamp, "2023-07-22_06-26-40");

	time_t Timestamp;
	EXPECT_TRUE(timestamp_from_str(aTimestamp, TimestampFormat::NOSPACE, &Timestamp));
	EXPECT_EQ(Timestamp, 1690000000);
}

TEST_F(TimestampTest, FromStrFailing)
{
	time_t Timestamp;
	// Invalid time string
	EXPECT_FALSE(timestamp_from_str("123 2023-12-31_12-58-55", TimestampFormat::NOSPACE, &Timestamp));

	// Invalid time string
	EXPECT_FALSE(timestamp_from_str("555-02-29_13-12-7", TimestampFormat::NOSPACE, &Timestamp));

	// Time string does not fit the format
	EXPECT_FALSE(timestamp_from_str("2004-05-15 18-13-53", TimestampFormat::SPACE, &Timestamp));

	// Invalid time string
	EXPECT_FALSE(timestamp_from_str("2000-01-01 00:00:00:00", TimestampFormat::SPACE, &Timestamp));
}

TEST_F(TimestampTest, WithSpecifiedFormatAndTimestamp)
{
	char aTimestamp[20];
	str_timestamp_ex(1704027535, aTimestamp, sizeof(aTimestamp), TimestampFormat::NOSPACE);
	EXPECT_STREQ(aTimestamp, "2023-12-31_12-58-55");

	str_timestamp_ex(1330520400, aTimestamp, sizeof(aTimestamp), TimestampFormat::NOSPACE);
	EXPECT_STREQ(aTimestamp, "2012-02-29_13-00-00");

	str_timestamp_ex(1084644833, aTimestamp, sizeof(aTimestamp), TimestampFormat::SPACE);
	EXPECT_STREQ(aTimestamp, "2004-05-15 18:13:53");
}
