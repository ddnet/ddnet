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
	EXPECT_TRUE(str_utf8_isspace(0x200b)); // Zero-width space
	EXPECT_TRUE(str_utf8_isspace(' '));
	EXPECT_FALSE(str_utf8_isspace('a'));
	// Control characters.
	for(char c = 0; c < 0x20; c++)
	{
		EXPECT_TRUE(str_utf8_isspace(c));
	}
}

TEST(Str, Utf8SkipWhitespaces)
{
	EXPECT_STREQ(str_utf8_skip_whitespaces("abc"), "abc");
	EXPECT_STREQ(str_utf8_skip_whitespaces("abc   "), "abc   ");
	EXPECT_STREQ(str_utf8_skip_whitespaces("    abc"), "abc");
	EXPECT_STREQ(str_utf8_skip_whitespaces("\xe2\x80\x8b abc"), "abc");
}

TEST(Str, Utf8TrimRight)
{
	char A1[] = "abc";
	str_utf8_trim_right(A1);
	EXPECT_STREQ(A1, "abc");
	char A2[] = "   abc";
	str_utf8_trim_right(A2);
	EXPECT_STREQ(A2, "   abc");
	char A3[] = "abc   ";
	str_utf8_trim_right(A3);
	EXPECT_STREQ(A3, "abc");
	char A4[] = "abc \xe2\x80\x8b";
	str_utf8_trim_right(A4);
	EXPECT_STREQ(A4, "abc");
}

TEST(Str, Utf8CompConfusables)
{
	EXPECT_TRUE(str_utf8_comp_confusable("abc", "abc") == 0);
	EXPECT_TRUE(str_utf8_comp_confusable("rn", "m") == 0);
	EXPECT_TRUE(str_utf8_comp_confusable("l", "ӏ") == 0); // CYRILLIC SMALL LETTER PALOCHKA
	EXPECT_FALSE(str_utf8_comp_confusable("o", "x") == 0);
	EXPECT_TRUE(str_utf8_comp_confusable("aceiou", "ąçęįǫų") == 0);
}

TEST(Str, Utf8ToLower)
{
	EXPECT_TRUE(str_utf8_tolower('A') == 'a');
	EXPECT_TRUE(str_utf8_tolower('z') == 'z');
	EXPECT_TRUE(str_utf8_tolower(192) == 224); // À -> à
	EXPECT_TRUE(str_utf8_tolower(7882) == 7883); // Ị -> ị

	EXPECT_TRUE(str_utf8_comp_nocase("ÖlÜ", "ölü") == 0);
	EXPECT_TRUE(str_utf8_comp_nocase("ÜlÖ", "ölü") > 0); // ü > ö
	EXPECT_TRUE(str_utf8_comp_nocase("ÖlÜ", "ölüa") < 0); // NULL < a
	EXPECT_TRUE(str_utf8_comp_nocase("ölüa", "ÖlÜ") > 0); // a < NULL

#if(CHAR_MIN < 0)
	const char a[2] = {CHAR_MIN, 0};
	const char b[2] = {0, 0};
	EXPECT_TRUE(str_utf8_comp_nocase(a, b) > 0);
	EXPECT_TRUE(str_utf8_comp_nocase(b, a) < 0);
#endif

	EXPECT_TRUE(str_utf8_comp_nocase_num("ÖlÜ", "ölüa", 5) == 0);
	EXPECT_TRUE(str_utf8_comp_nocase_num("ÖlÜ", "ölüa", 6) != 0);
	EXPECT_TRUE(str_utf8_comp_nocase_num("a", "z", 0) == 0);
	EXPECT_TRUE(str_utf8_comp_nocase_num("a", "z", 1) != 0);

	const char str[] = "ÄÖÜ";
	EXPECT_TRUE(str_utf8_find_nocase(str, "ä") == str);
	EXPECT_TRUE(str_utf8_find_nocase(str, "ö") == str + 2);
	EXPECT_TRUE(str_utf8_find_nocase(str, "ü") == str + 4);
	EXPECT_TRUE(str_utf8_find_nocase(str, "z") == NULL);
}

TEST(Str, Startswith)
{
	EXPECT_TRUE(str_startswith("abcdef", "abc"));
	EXPECT_FALSE(str_startswith("abc", "abcdef"));

	EXPECT_TRUE(str_startswith("xyz", ""));
	EXPECT_FALSE(str_startswith("", "xyz"));

	EXPECT_FALSE(str_startswith("house", "home"));
	EXPECT_FALSE(str_startswith("blackboard", "board"));

	EXPECT_TRUE(str_startswith("поплавать", "по"));
	EXPECT_FALSE(str_startswith("плавать", "по"));

	static const char ABCDEFG[] = "abcdefg";
	static const char ABC[] = "abc";
	EXPECT_EQ(str_startswith(ABCDEFG, ABC) - ABCDEFG, str_length(ABC));
}

TEST(Str, Endswith)
{
	EXPECT_TRUE(str_endswith("abcdef", "def"));
	EXPECT_FALSE(str_endswith("def", "abcdef"));

	EXPECT_TRUE(str_endswith("xyz", ""));
	EXPECT_FALSE(str_endswith("", "xyz"));

	EXPECT_FALSE(str_endswith("rhyme", "mine"));
	EXPECT_FALSE(str_endswith("blackboard", "black"));

	EXPECT_TRUE(str_endswith("люди", "юди"));
	EXPECT_FALSE(str_endswith("люди", "любовь"));

	static const char ABCDEFG[] = "abcdefg";
	static const char DEFG[] = "defg";
	EXPECT_EQ(str_endswith(ABCDEFG, DEFG) - ABCDEFG,
		str_length(ABCDEFG) - str_length(DEFG));
}

TEST(Str, HexDecode)
{
	char aOut[5] = {'a', 'b', 'c', 'd', 0};
	EXPECT_EQ(str_hex_decode(aOut, 0, ""), 0);
	EXPECT_STREQ(aOut, "abcd");
	EXPECT_EQ(str_hex_decode(aOut, 0, " "), 2);
	EXPECT_STREQ(aOut, "abcd");
	EXPECT_EQ(str_hex_decode(aOut, 1, "1"), 2);
	EXPECT_STREQ(aOut + 1, "bcd");
	EXPECT_EQ(str_hex_decode(aOut, 1, "41"), 0);
	EXPECT_STREQ(aOut, "Abcd");
	EXPECT_EQ(str_hex_decode(aOut, 1, "4x"), 1);
	EXPECT_STREQ(aOut + 1, "bcd");
	EXPECT_EQ(str_hex_decode(aOut, 1, "x1"), 1);
	EXPECT_STREQ(aOut + 1, "bcd");
	EXPECT_EQ(str_hex_decode(aOut, 1, "411"), 2);
	EXPECT_STREQ(aOut + 1, "bcd");
	EXPECT_EQ(str_hex_decode(aOut, 4, "41424344"), 0);
	EXPECT_STREQ(aOut, "ABCD");
}

TEST(Str, Tokenize)
{
	char aTest[] = "GER,RUS,ZAF,BRA,CAN";
	const char *aOut[] = {"GER", "RUS", "ZAF", "BRA", "CAN"};
	char aBuf[4];

	int n = 0;
	for(const char *tok = aTest; (tok = str_next_token(tok, ",", aBuf, sizeof(aBuf)));)
		EXPECT_STREQ(aOut[n++], aBuf);

	char aTest2[] = "";
	EXPECT_EQ(str_next_token(aTest2, ",", aBuf, sizeof(aBuf)), nullptr);

	char aTest3[] = "+b";
	str_next_token(aTest3, "+", aBuf, sizeof(aBuf));
	EXPECT_STREQ(aBuf, "b");
}

TEST(Str, InList)
{
	char aTest[] = "GER,RUS,ZAF,BRA,CAN";
	EXPECT_TRUE(str_in_list(aTest, ",", "GER"));
	EXPECT_TRUE(str_in_list(aTest, ",", "RUS"));
	EXPECT_TRUE(str_in_list(aTest, ",", "ZAF"));
	EXPECT_TRUE(str_in_list(aTest, ",", "BRA"));
	EXPECT_TRUE(str_in_list(aTest, ",", "CAN"));

	EXPECT_FALSE(str_in_list(aTest, ",", "CHN"));
	EXPECT_FALSE(str_in_list(aTest, ",", "R,R"));

	EXPECT_FALSE(str_in_list("abc,xyz", ",", "abcdef"));
	EXPECT_FALSE(str_in_list("", ",", ""));
	EXPECT_FALSE(str_in_list("", ",", "xyz"));

	EXPECT_TRUE(str_in_list("FOO,,BAR", ",", ""));
	EXPECT_TRUE(str_in_list("abc,,def", ",", "def"));
}

TEST(Str, StrFormat)
{
	char aBuf[4];
	EXPECT_EQ(str_format(aBuf, 4, "%d:", 9), 2);
	EXPECT_STREQ(aBuf, "9:");
	EXPECT_EQ(str_format(aBuf, 4, "%d: ", 9), 3);
	EXPECT_STREQ(aBuf, "9: ");
	EXPECT_EQ(str_format(aBuf, 4, "%d: ", 99), 3);
	EXPECT_STREQ(aBuf, "99:");
}

TEST(Str, StrCopyNum)
{
	const char *foo = "Foobaré";
	char aBuf[64];
	str_utf8_truncate(aBuf, 3, foo, 1);
	EXPECT_STREQ(aBuf, "F");
	str_utf8_truncate(aBuf, 3, foo, 2);
	EXPECT_STREQ(aBuf, "Fo");
	str_utf8_truncate(aBuf, 3, foo, 3);
	EXPECT_STREQ(aBuf, "Fo");
	str_utf8_truncate(aBuf, sizeof(aBuf), foo, 6);
	EXPECT_STREQ(aBuf, "Foobar");
	str_utf8_truncate(aBuf, sizeof(aBuf), foo, 7);
	EXPECT_STREQ(aBuf, "Foobaré");

	char aBuf2[8];
	str_utf8_truncate(aBuf2, sizeof(aBuf2), foo, 7);
	EXPECT_STREQ(aBuf2, "Foobar");
	char aBuf3[9];
	str_utf8_truncate(aBuf3, sizeof(aBuf3), foo, 7);
	EXPECT_STREQ(aBuf3, "Foobaré");
}

TEST(Str, StrCopyUtf8)
{
	const char *foo = "DDNet最好了";
	char aBuf[64];
	str_utf8_copy(aBuf, foo, 7);
	EXPECT_STREQ(aBuf, "DDNet");
	str_utf8_copy(aBuf, foo, 8);
	EXPECT_STREQ(aBuf, "DDNet");
	str_utf8_copy(aBuf, foo, 9);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_utf8_copy(aBuf, foo, 10);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_utf8_copy(aBuf, foo, 11);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_utf8_copy(aBuf, foo, 12);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_utf8_copy(aBuf, foo, 13);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_utf8_copy(aBuf, foo, 14);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_utf8_copy(aBuf, foo, 15);
	EXPECT_STREQ(aBuf, "DDNet最好了");
	str_utf8_copy(aBuf, foo, 16);
	EXPECT_STREQ(aBuf, "DDNet最好了");
}
