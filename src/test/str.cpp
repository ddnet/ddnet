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
