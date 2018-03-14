#include <gtest/gtest.h>

#include <base/system.h>

TEST(Str, Dist)
{
	EXPECT_EQ(str_utf8_dist("aaa", "aaa"), 0);
	EXPECT_EQ(str_utf8_dist("123", "123"), 0);
	EXPECT_EQ(str_utf8_dist("", ""), 0);
	EXPECT_EQ(str_utf8_dist("a", "b"), 1);
	EXPECT_EQ(str_utf8_dist("", "aaa"), 3);
	EXPECT_EQ(str_utf8_dist("123", ""), 3);
	EXPECT_EQ(str_utf8_dist("ä", ""), 1);
	EXPECT_EQ(str_utf8_dist("Hëllö", "Hello"), 2);
	// https://en.wikipedia.org/w/index.php?title=Levenshtein_distance&oldid=828480025#Example
	EXPECT_EQ(str_utf8_dist("kitten", "sitting"), 3);
	EXPECT_EQ(str_utf8_dist("flaw", "lawn"), 2);
	EXPECT_EQ(str_utf8_dist("saturday", "sunday"), 3);
}

TEST(Str, Utf8Isspace)
{
	EXPECT_TRUE(str_utf8_isspace(' '));
	EXPECT_FALSE(str_utf8_isspace('a'));
}

TEST(Str, Utf8SkipWhitespaces)
{
	EXPECT_STREQ(str_utf8_skip_whitespaces("abc"), "abc");
	EXPECT_STREQ(str_utf8_skip_whitespaces("abc   "), "abc   ");
	EXPECT_STREQ(str_utf8_skip_whitespaces("    abc"), "abc");
}

TEST(Str, Utf8TrimRight)
{
	char A1[] = "abc"; str_utf8_trim_right(A1); EXPECT_STREQ(A1, "abc");
	char A2[] = "   abc"; str_utf8_trim_right(A2); EXPECT_STREQ(A2, "   abc");
	char A3[] = "abc   "; str_utf8_trim_right(A3); EXPECT_STREQ(A3, "abc");
}
