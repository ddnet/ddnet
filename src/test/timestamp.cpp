#include <gtest/gtest.h>

#include <base/system.h>
#include <cstdlib>

class TimestampTest : public testing::Test
{
protected:
	void SetUp() override
	{
#if defined(CONF_FAMILY_WINDOWS)
		_putenv_s("TZ", "UTC");
		_tzset();
#else
		setenv("TZ", "UTC", 1);
		tzset();
#endif
	}
};

TEST_F(TimestampTest, FromStr)
{
	time_t Timestamp;
	EXPECT_TRUE(timestamp_from_str("2023-12-31_12-58-55", FORMAT_NOSPACE, &Timestamp));
	EXPECT_EQ(Timestamp, 1704027535);

	EXPECT_TRUE(timestamp_from_str("2012-02-29_13-00-00", FORMAT_NOSPACE, &Timestamp));
	EXPECT_EQ(Timestamp, 1330520400);

	EXPECT_TRUE(timestamp_from_str("2004-05-15 18:13:53", FORMAT_SPACE, &Timestamp));
	EXPECT_EQ(Timestamp, 1084644833);
}

TEST_F(TimestampTest, FromStrFailing)
{
	time_t Timestamp;
	// Invalid time string
	EXPECT_FALSE(timestamp_from_str("123 2023-12-31_12-58-55", FORMAT_NOSPACE, &Timestamp));

	// Invalid time string
	EXPECT_FALSE(timestamp_from_str("555-02-29_13-12-7", FORMAT_NOSPACE, &Timestamp));

	// Time string does not fit the format
	EXPECT_FALSE(timestamp_from_str("2004-05-15 18-13-53", FORMAT_SPACE, &Timestamp));

	// Invalid time string
	EXPECT_FALSE(timestamp_from_str("2000-01-01 00:00:00:00", FORMAT_SPACE, &Timestamp));
}

TEST_F(TimestampTest, WithSpecifiedFormatAndTimestamp)
{
	char aTimestamp[20];
	str_timestamp_ex(1704027535, aTimestamp, sizeof(aTimestamp), FORMAT_NOSPACE);
	EXPECT_STREQ(aTimestamp, "2023-12-31_12-58-55");

	str_timestamp_ex(1330520400, aTimestamp, sizeof(aTimestamp), FORMAT_NOSPACE);
	EXPECT_STREQ(aTimestamp, "2012-02-29_13-00-00");

	str_timestamp_ex(1084644833, aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);
	EXPECT_STREQ(aTimestamp, "2004-05-15 18:13:53");
}
