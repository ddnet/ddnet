#include <gtest/gtest.h>

#include <base/system.h>

#include <game/gamecore.h>

TEST(Str, StrDelim)
{
	int Start, End;
	//                                      0123456
	//                            01234567891111111
	EXPECT_EQ(str_delimiters_around_offset("123;123456789;aaa", ";", 5, &Start, &End), true);
	EXPECT_EQ(Start, 4);
	EXPECT_EQ(End, 13);

	EXPECT_EQ(str_delimiters_around_offset("123;123", ";", 1, &Start, &End), false);
	EXPECT_EQ(Start, 0);
	EXPECT_EQ(End, 3);

	EXPECT_EQ(str_delimiters_around_offset("---foo---bar---baz---hello", "---", 1, &Start, &End), false);
	EXPECT_EQ(Start, 0);
	EXPECT_EQ(End, 0);

	EXPECT_EQ(str_delimiters_around_offset("---foo---bar---baz---hello", "---", 2, &Start, &End), false);
	EXPECT_EQ(Start, 0);
	EXPECT_EQ(End, 0);

	EXPECT_EQ(str_delimiters_around_offset("---foo---bar---baz---hello", "---", 3, &Start, &End), true);
	EXPECT_EQ(Start, 3);
	EXPECT_EQ(End, 6);

	EXPECT_EQ(str_delimiters_around_offset("---foo---bar---baz---hello", "---", 4, &Start, &End), true);
	EXPECT_EQ(Start, 3);
	EXPECT_EQ(End, 6);

	EXPECT_EQ(str_delimiters_around_offset("---foo---bar---baz---hello", "---", 9, &Start, &End), true);
	EXPECT_EQ(Start, 9);
	EXPECT_EQ(End, 12);

	EXPECT_EQ(str_delimiters_around_offset("---foo---bar---baz---hello", "---", 22, &Start, &End), false);
	EXPECT_EQ(Start, 21);
	EXPECT_EQ(End, 26);

	EXPECT_EQ(str_delimiters_around_offset("foo;;;;bar;;;;;;", ";", 2, &Start, &End), false);
	EXPECT_EQ(Start, 0);
	EXPECT_EQ(End, 3);

	EXPECT_EQ(str_delimiters_around_offset("foo;;;;bar;;;;;;", ";", 3, &Start, &End), false);
	EXPECT_EQ(Start, 0);
	EXPECT_EQ(End, 3);

	EXPECT_EQ(str_delimiters_around_offset("foo;;;;bar;;;;;;", ";", 4, &Start, &End), true);
	EXPECT_EQ(Start, 4);
	EXPECT_EQ(End, 4);

	EXPECT_EQ(str_delimiters_around_offset("", ";", 4, &Start, &End), false);
	EXPECT_EQ(Start, 0);
	EXPECT_EQ(End, 0);
}

TEST(Str, StrIsNum)
{
	EXPECT_EQ(str_isnum('/'), false);
	EXPECT_EQ(str_isnum('0'), true);
	EXPECT_EQ(str_isnum('1'), true);
	EXPECT_EQ(str_isnum('2'), true);
	EXPECT_EQ(str_isnum('8'), true);
	EXPECT_EQ(str_isnum('9'), true);
	EXPECT_EQ(str_isnum(':'), false);
	EXPECT_EQ(str_isnum(' '), false);
}

TEST(Str, StrIsAllNum)
{
	EXPECT_EQ(str_isallnum("/"), 0);
	EXPECT_EQ(str_isallnum("0"), 1);
	EXPECT_EQ(str_isallnum("1"), 1);
	EXPECT_EQ(str_isallnum("2"), 1);
	EXPECT_EQ(str_isallnum("8"), 1);
	EXPECT_EQ(str_isallnum("9"), 1);
	EXPECT_EQ(str_isallnum(":"), 0);
	EXPECT_EQ(str_isallnum(" "), 0);

	EXPECT_EQ(str_isallnum("123"), 1);
	EXPECT_EQ(str_isallnum("123/"), 0);
	EXPECT_EQ(str_isallnum("123:"), 0);
}

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
	EXPECT_TRUE(str_utf8_comp_confusable("m", "rn") == 0);
	EXPECT_TRUE(str_utf8_comp_confusable("rna", "ma") == 0);
	EXPECT_TRUE(str_utf8_comp_confusable("ma", "rna") == 0);
	EXPECT_FALSE(str_utf8_comp_confusable("mA", "rna") == 0);
	EXPECT_FALSE(str_utf8_comp_confusable("ma", "rnA") == 0);
	EXPECT_TRUE(str_utf8_comp_confusable("arn", "am") == 0);
	EXPECT_TRUE(str_utf8_comp_confusable("am", "arn") == 0);
	EXPECT_FALSE(str_utf8_comp_confusable("Am", "arn") == 0);
	EXPECT_FALSE(str_utf8_comp_confusable("am", "Arn") == 0);
	EXPECT_TRUE(str_utf8_comp_confusable("l", "ӏ") == 0); // CYRILLIC SMALL LETTER PALOCHKA
	EXPECT_TRUE(str_utf8_comp_confusable("i", "¡") == 0); // INVERTED EXCLAMATION MARK
	EXPECT_FALSE(str_utf8_comp_confusable("o", "x") == 0);
	EXPECT_TRUE(str_utf8_comp_confusable("aceiou", "ąçęįǫų") == 0);
}

TEST(Str, Utf8ToSkeleton)
{
	int aBuf[32];
	EXPECT_EQ(str_utf8_to_skeleton("abc", aBuf, 0), 0);
	EXPECT_EQ(str_utf8_to_skeleton("", aBuf, std::size(aBuf)), 0);
	EXPECT_EQ(str_utf8_to_skeleton("abc", aBuf, std::size(aBuf)), 3);
	EXPECT_EQ(aBuf[0], 'a');
	EXPECT_EQ(aBuf[1], 'b');
	EXPECT_EQ(aBuf[2], 'c');
	EXPECT_EQ(str_utf8_to_skeleton("m", aBuf, std::size(aBuf)), 2);
	EXPECT_EQ(aBuf[0], 'r');
	EXPECT_EQ(aBuf[1], 'n');
	EXPECT_EQ(str_utf8_to_skeleton("rn", aBuf, std::size(aBuf)), 2);
	EXPECT_EQ(aBuf[0], 'r');
	EXPECT_EQ(aBuf[1], 'n');
	EXPECT_EQ(str_utf8_to_skeleton("ӏ", aBuf, std::size(aBuf)), 1); // CYRILLIC SMALL LETTER PALOCHKA
	EXPECT_EQ(aBuf[0], 'i');
	EXPECT_EQ(str_utf8_to_skeleton("¡", aBuf, std::size(aBuf)), 1); // INVERTED EXCLAMATION MARK
	EXPECT_EQ(aBuf[0], 'i');
	EXPECT_EQ(str_utf8_to_skeleton("ąçęįǫų", aBuf, std::size(aBuf)), 6);
	EXPECT_EQ(aBuf[0], 'a');
	EXPECT_EQ(aBuf[1], 'c');
	EXPECT_EQ(aBuf[2], 'e');
	EXPECT_EQ(aBuf[3], 'i');
	EXPECT_EQ(aBuf[4], 'o');
	EXPECT_EQ(aBuf[5], 'u');
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
}

TEST(Str, Utf8FindNocase)
{
	const char *pStr = "abc";
	const char *pEnd;
	EXPECT_EQ(str_utf8_find_nocase(pStr, "a", &pEnd), pStr);
	EXPECT_EQ(pEnd, pStr + str_length("a"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "b", &pEnd), pStr + str_length("a"));
	EXPECT_EQ(pEnd, pStr + str_length("ab"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "c", &pEnd), pStr + str_length("ab"));
	EXPECT_EQ(pEnd, pStr + str_length("abc"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "d", &pEnd), nullptr);
	EXPECT_EQ(pEnd, nullptr);

	EXPECT_EQ(str_utf8_find_nocase(pStr, "A", &pEnd), pStr);
	EXPECT_EQ(pEnd, pStr + str_length("a"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "B", &pEnd), pStr + str_length("a"));
	EXPECT_EQ(pEnd, pStr + str_length("ab"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "C", &pEnd), pStr + str_length("ab"));
	EXPECT_EQ(pEnd, pStr + str_length("abc"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "D", &pEnd), nullptr);
	EXPECT_EQ(pEnd, nullptr);

	pStr = "ÄÖÜ";
	EXPECT_EQ(str_utf8_find_nocase(pStr, "ä", &pEnd), pStr);
	EXPECT_EQ(pEnd, pStr + str_length("Ä"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "ö", &pEnd), pStr + str_length("Ä"));
	EXPECT_EQ(pEnd, pStr + str_length("ÄÖ"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "ü", &pEnd), pStr + str_length("ÄÖ"));
	EXPECT_EQ(pEnd, pStr + str_length("ÄÖÜ"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "z", &pEnd), nullptr);
	EXPECT_EQ(pEnd, nullptr);

	// Both 'I' and 'İ' map to 'i'
	pStr = "antimatter";
	EXPECT_EQ(str_utf8_find_nocase(pStr, "I", &pEnd), pStr + str_length("ant"));
	EXPECT_EQ(pEnd, pStr + str_length("anti"));
	EXPECT_EQ(str_utf8_find_nocase(pStr, "İ", &pEnd), pStr + str_length("ant"));
	EXPECT_EQ(pEnd, pStr + str_length("anti"));
	pStr = "ANTIMATTER";
	EXPECT_EQ(str_utf8_find_nocase(pStr, "i", &pEnd), pStr + str_length("ANT"));
	EXPECT_EQ(pEnd, pStr + str_length("ANTI"));
	pStr = "ANTİMATTER";
	EXPECT_EQ(str_utf8_find_nocase(pStr, "i", &pEnd), pStr + str_length("ANT"));
	EXPECT_EQ(pEnd, pStr + str_length("ANTİ"));
}

TEST(Str, Utf8FixTruncation)
{
	char aaBuf[][32] = {
		"",
		"\xff",
		"abc",
		"abc\xff",
		"blub\xffxyz",
		"привет Наташа\xff",
		"до свидания\xffОлег",
	};
	const char *apExpected[] = {
		"",
		"",
		"abc",
		"abc",
		"blub\xffxyz",
		"привет Наташа",
		"до свидания\xffОлег",
	};
	for(unsigned i = 0; i < std::size(aaBuf); i++)
	{
		EXPECT_EQ(str_utf8_fix_truncation(aaBuf[i]), str_length(apExpected[i]));
		EXPECT_STREQ(aaBuf[i], apExpected[i]);
	}
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

TEST(Str, StartswithNocase)
{
	EXPECT_TRUE(str_startswith_nocase("Abcdef", "abc"));
	EXPECT_FALSE(str_startswith_nocase("aBc", "abcdef"));

	EXPECT_TRUE(str_startswith_nocase("xYz", ""));
	EXPECT_FALSE(str_startswith_nocase("", "xYz"));

	EXPECT_FALSE(str_startswith_nocase("house", "home"));
	EXPECT_FALSE(str_startswith_nocase("Blackboard", "board"));

	EXPECT_TRUE(str_startswith_nocase("поплавать", "по"));
	EXPECT_FALSE(str_startswith_nocase("плавать", "по"));

	static const char ABCDEFG[] = "aBcdefg";
	static const char ABC[] = "abc";
	EXPECT_EQ(str_startswith_nocase(ABCDEFG, ABC) - ABCDEFG, str_length(ABC));
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

TEST(StrFormat, Positional)
{
	char aBuf[256];

	// normal
	str_format(aBuf, sizeof(aBuf), "%s %s", "first", "second");
	EXPECT_STREQ(aBuf, "first second");

	// normal with positional arguments
	str_format(aBuf, sizeof(aBuf), "%1$s %2$s", "first", "second");
	EXPECT_STREQ(aBuf, "first second");

	// reverse
	str_format(aBuf, sizeof(aBuf), "%2$s %1$s", "first", "second");
	EXPECT_STREQ(aBuf, "second first");

	// duplicate
	str_format(aBuf, sizeof(aBuf), "%1$s %1$s %2$d %1$s %2$d", "str", 1);
	EXPECT_STREQ(aBuf, "str str 1 str 1");
}

TEST(Str, EndswithNocase)
{
	EXPECT_TRUE(str_endswith_nocase("abcdef", "deF"));
	EXPECT_FALSE(str_endswith_nocase("def", "abcdef"));

	EXPECT_TRUE(str_endswith_nocase("xyz", ""));
	EXPECT_FALSE(str_endswith_nocase("", "xyz"));

	EXPECT_FALSE(str_endswith_nocase("rhyme", "minE"));
	EXPECT_FALSE(str_endswith_nocase("blackboard", "black"));

	EXPECT_TRUE(str_endswith_nocase("люди", "юди"));
	EXPECT_FALSE(str_endswith_nocase("люди", "любовь"));

	static const char ABCDEFG[] = "abcdefG";
	static const char DEFG[] = "defg";
	EXPECT_EQ(str_endswith_nocase(ABCDEFG, DEFG) - ABCDEFG,
		str_length(ABCDEFG) - str_length(DEFG));
}

TEST(Str, HexEncode)
{
	char aOut[64];
	const char *pData = "ABCD";
	str_hex(aOut, sizeof(aOut), pData, 0);
	EXPECT_STREQ(aOut, "");
	str_hex(aOut, sizeof(aOut), pData, 1);
	EXPECT_STREQ(aOut, "41 ");
	str_hex(aOut, sizeof(aOut), pData, 2);
	EXPECT_STREQ(aOut, "41 42 ");
	str_hex(aOut, sizeof(aOut), pData, 3);
	EXPECT_STREQ(aOut, "41 42 43 ");
	str_hex(aOut, sizeof(aOut), pData, 4);
	EXPECT_STREQ(aOut, "41 42 43 44 ");

	str_hex(aOut, 1, pData, 4);
	EXPECT_STREQ(aOut, "");
	str_hex(aOut, 2, pData, 4);
	EXPECT_STREQ(aOut, "");
	str_hex(aOut, 3, pData, 4);
	EXPECT_STREQ(aOut, "");
	str_hex(aOut, 4, pData, 4);
	EXPECT_STREQ(aOut, "41 ");
	str_hex(aOut, 5, pData, 4);
	EXPECT_STREQ(aOut, "41 ");
	str_hex(aOut, 6, pData, 4);
	EXPECT_STREQ(aOut, "41 ");
	str_hex(aOut, 7, pData, 4);
	EXPECT_STREQ(aOut, "41 42 ");
	str_hex(aOut, 8, pData, 4);
	EXPECT_STREQ(aOut, "41 42 ");
}

TEST(Str, HexEncodeCstyle)
{
	char aOut[128];
	const char *pData = "ABCD";
	str_hex_cstyle(aOut, sizeof(aOut), pData, 0);
	EXPECT_STREQ(aOut, "");
	str_hex_cstyle(aOut, sizeof(aOut), pData, 1);
	EXPECT_STREQ(aOut, "0x41");
	str_hex_cstyle(aOut, sizeof(aOut), pData, 2);
	EXPECT_STREQ(aOut, "0x41, 0x42");
	str_hex_cstyle(aOut, sizeof(aOut), pData, 3);
	EXPECT_STREQ(aOut, "0x41, 0x42, 0x43");
	str_hex_cstyle(aOut, sizeof(aOut), pData, 4);
	EXPECT_STREQ(aOut, "0x41, 0x42, 0x43, 0x44");

	str_hex_cstyle(aOut, 1, pData, 4);
	EXPECT_STREQ(aOut, "");
	str_hex_cstyle(aOut, 2, pData, 4);
	EXPECT_STREQ(aOut, "");
	str_hex_cstyle(aOut, 3, pData, 4);
	EXPECT_STREQ(aOut, "");
	str_hex_cstyle(aOut, 4, pData, 4);
	EXPECT_STREQ(aOut, "");
	str_hex_cstyle(aOut, 5, pData, 4);
	EXPECT_STREQ(aOut, "");
	str_hex_cstyle(aOut, 6, pData, 4);
	EXPECT_STREQ(aOut, "");
	str_hex_cstyle(aOut, 7, pData, 4);
	EXPECT_STREQ(aOut, "0x41");
	str_hex_cstyle(aOut, 12, pData, 4);
	EXPECT_STREQ(aOut, "0x41");
	str_hex_cstyle(aOut, 13, pData, 4);
	EXPECT_STREQ(aOut, "0x41, 0x42");
	str_hex_cstyle(aOut, 14, pData, 4);
	EXPECT_STREQ(aOut, "0x41, 0x42");

	str_hex_cstyle(aOut, sizeof(aOut), pData, 4, 1);
	EXPECT_STREQ(aOut, "0x41,\n0x42,\n0x43,\n0x44");
	str_hex_cstyle(aOut, sizeof(aOut), pData, 4, 2);
	EXPECT_STREQ(aOut, "0x41, 0x42,\n0x43, 0x44");
	str_hex_cstyle(aOut, sizeof(aOut), pData, 4, 3);
	EXPECT_STREQ(aOut, "0x41, 0x42, 0x43,\n0x44");
	str_hex_cstyle(aOut, sizeof(aOut), pData, 4, 4);
	EXPECT_STREQ(aOut, "0x41, 0x42, 0x43, 0x44");
	str_hex_cstyle(aOut, sizeof(aOut), pData, 4, 500);
	EXPECT_STREQ(aOut, "0x41, 0x42, 0x43, 0x44");
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

void StrBase64Str(char *pBuffer, int BufferSize, const char *pString)
{
	str_base64(pBuffer, BufferSize, pString, str_length(pString));
}

TEST(Str, Base64)
{
	char aBuf[128];
	str_base64(aBuf, sizeof(aBuf), "\0", 1);
	EXPECT_STREQ(aBuf, "AA==");
	str_base64(aBuf, sizeof(aBuf), "\0\0", 2);
	EXPECT_STREQ(aBuf, "AAA=");
	str_base64(aBuf, sizeof(aBuf), "\0\0\0", 3);
	EXPECT_STREQ(aBuf, "AAAA");

	StrBase64Str(aBuf, sizeof(aBuf), "");
	EXPECT_STREQ(aBuf, "");

	// https://en.wikipedia.org/w/index.php?title=Base64&oldid=1033503483#Output_padding
	StrBase64Str(aBuf, sizeof(aBuf), "pleasure.");
	EXPECT_STREQ(aBuf, "cGxlYXN1cmUu");
	StrBase64Str(aBuf, sizeof(aBuf), "leasure.");
	EXPECT_STREQ(aBuf, "bGVhc3VyZS4=");
	StrBase64Str(aBuf, sizeof(aBuf), "easure.");
	EXPECT_STREQ(aBuf, "ZWFzdXJlLg==");
	StrBase64Str(aBuf, sizeof(aBuf), "asure.");
	EXPECT_STREQ(aBuf, "YXN1cmUu");
	StrBase64Str(aBuf, sizeof(aBuf), "sure.");
	EXPECT_STREQ(aBuf, "c3VyZS4=");
}

TEST(Str, Base64Decode)
{
	char aOut[17];
	str_copy(aOut, "XXXXXXXXXXXXXXXX", sizeof(aOut));
	EXPECT_EQ(str_base64_decode(aOut, sizeof(aOut), ""), 0);
	EXPECT_STREQ(aOut, "XXXXXXXXXXXXXXXX");

	// https://en.wikipedia.org/w/index.php?title=Base64&oldid=1033503483#Output_padding
	str_copy(aOut, "XXXXXXXXXXXXXXXX", sizeof(aOut));
	EXPECT_EQ(str_base64_decode(aOut, sizeof(aOut), "cGxlYXN1cmUu"), 9);
	EXPECT_STREQ(aOut, "pleasure.XXXXXXX");
	str_copy(aOut, "XXXXXXXXXXXXXXXX", sizeof(aOut));
	EXPECT_EQ(str_base64_decode(aOut, sizeof(aOut), "bGVhc3VyZS4="), 8);
	EXPECT_STREQ(aOut, "leasure.XXXXXXXX");
	str_copy(aOut, "XXXXXXXXXXXXXXXX", sizeof(aOut));
	EXPECT_EQ(str_base64_decode(aOut, sizeof(aOut), "ZWFzdXJlLg=="), 7);
	EXPECT_STREQ(aOut, "easure.XXXXXXXXX");
	str_copy(aOut, "XXXXXXXXXXXXXXXX", sizeof(aOut));
	EXPECT_EQ(str_base64_decode(aOut, sizeof(aOut), "YXN1cmUu"), 6);
	EXPECT_STREQ(aOut, "asure.XXXXXXXXXX");
	str_copy(aOut, "XXXXXXXXXXXXXXXX", sizeof(aOut));
	EXPECT_EQ(str_base64_decode(aOut, sizeof(aOut), "c3VyZS4="), 5);
	EXPECT_STREQ(aOut, "sure.XXXXXXXXXXX");
	str_copy(aOut, "XXXXXXXXXXXXXXXX", sizeof(aOut));
	EXPECT_EQ(str_base64_decode(aOut, sizeof(aOut), "////"), 3);
	EXPECT_STREQ(aOut, "\xff\xff\xffXXXXXXXXXXXXX");
}

TEST(Str, Base64DecodeError)
{
	char aBuf[128];
	// Wrong padding.
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "A"), 0);
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "AA"), 0);
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "AAA"), 0);
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "A==="), 0);
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "=AAA"), 0);
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "===="), 0);
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "AAA=AAAA"), 0);
	// Invalid characters.
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "----"), 0);
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "AAAA "), 0);
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "AAA "), 0);
	// Invalid padding values.
	EXPECT_LT(str_base64_decode(aBuf, sizeof(aBuf), "//=="), 0);
}

TEST(Str, Tokenize)
{
	char aTest[] = "GER,RUS,ZAF,BRA,CAN";
	const char *apOut[] = {"GER", "RUS", "ZAF", "BRA", "CAN"};
	char aBuf[4];

	int n = 0;
	for(const char *pTok = aTest; (pTok = str_next_token(pTok, ",", aBuf, sizeof(aBuf)));)
		EXPECT_STREQ(apOut[n++], aBuf);

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

TEST(Str, Format)
{
	char aBuf[4];
	EXPECT_EQ(str_format(aBuf, 4, "%d:", 9), 2);
	EXPECT_STREQ(aBuf, "9:");
	EXPECT_EQ(str_format(aBuf, 4, "%d: ", 9), 3);
	EXPECT_STREQ(aBuf, "9: ");
	EXPECT_EQ(str_format(aBuf, 4, "%d: ", 99), 3);
	EXPECT_STREQ(aBuf, "99:");
}

TEST(Str, FormatTruncate)
{
	const char *pStr = "DDNet最好了";
	char aBuf[64];
	str_format(aBuf, 7, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet");
	str_format(aBuf, 8, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet");
	str_format(aBuf, 9, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_format(aBuf, 10, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_format(aBuf, 11, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_format(aBuf, 12, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_format(aBuf, 13, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_format(aBuf, 14, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_format(aBuf, 15, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet最好了");
	str_format(aBuf, 16, "%s", pStr);
	EXPECT_STREQ(aBuf, "DDNet最好了");
}

TEST(Str, TrimWords)
{
	const char *pStr1 = "aa bb ccc   dddd    eeeee";
	EXPECT_STREQ(str_trim_words(pStr1, 0), "aa bb ccc   dddd    eeeee");
	EXPECT_STREQ(str_trim_words(pStr1, 1), "bb ccc   dddd    eeeee");
	EXPECT_STREQ(str_trim_words(pStr1, 2), "ccc   dddd    eeeee");
	EXPECT_STREQ(str_trim_words(pStr1, 3), "dddd    eeeee");
	EXPECT_STREQ(str_trim_words(pStr1, 4), "eeeee");
	EXPECT_STREQ(str_trim_words(pStr1, 5), "");
	EXPECT_STREQ(str_trim_words(pStr1, 100), "");
	const char *pStr2 = "   aaa  bb   ";
	EXPECT_STREQ(str_trim_words(pStr2, 0), "aaa  bb   ");
	EXPECT_STREQ(str_trim_words(pStr2, 1), "bb   ");
	EXPECT_STREQ(str_trim_words(pStr2, 2), "");
	EXPECT_STREQ(str_trim_words(pStr2, 100), "");
	const char *pStr3 = "\n\naa  bb\t\tccc\r\n\r\ndddd";
	EXPECT_STREQ(str_trim_words(pStr3, 0), "aa  bb\t\tccc\r\n\r\ndddd");
	EXPECT_STREQ(str_trim_words(pStr3, 1), "bb\t\tccc\r\n\r\ndddd");
	EXPECT_STREQ(str_trim_words(pStr3, 2), "ccc\r\n\r\ndddd");
	EXPECT_STREQ(str_trim_words(pStr3, 3), "dddd");
	EXPECT_STREQ(str_trim_words(pStr3, 4), "");
	EXPECT_STREQ(str_trim_words(pStr3, 100), "");
}

TEST(Str, CopyNum)
{
	const char *pFoo = "Foobaré";
	char aBuf[64];
	str_utf8_truncate(aBuf, 3, pFoo, 1);
	EXPECT_STREQ(aBuf, "F");
	str_utf8_truncate(aBuf, 3, pFoo, 2);
	EXPECT_STREQ(aBuf, "Fo");
	str_utf8_truncate(aBuf, 3, pFoo, 3);
	EXPECT_STREQ(aBuf, "Fo");
	str_utf8_truncate(aBuf, sizeof(aBuf), pFoo, 6);
	EXPECT_STREQ(aBuf, "Foobar");
	str_utf8_truncate(aBuf, sizeof(aBuf), pFoo, 7);
	EXPECT_STREQ(aBuf, "Foobaré");
	str_utf8_truncate(aBuf, sizeof(aBuf), pFoo, 0);
	EXPECT_STREQ(aBuf, "");

	char aBuf2[8];
	str_utf8_truncate(aBuf2, sizeof(aBuf2), pFoo, 7);
	EXPECT_STREQ(aBuf2, "Foobar");
	char aBuf3[9];
	str_utf8_truncate(aBuf3, sizeof(aBuf3), pFoo, 7);
	EXPECT_STREQ(aBuf3, "Foobaré");
}

TEST(Str, Copy)
{
	const char *pStr = "DDNet最好了";
	char aBuf[64];
	str_copy(aBuf, pStr, 7);
	EXPECT_STREQ(aBuf, "DDNet");
	str_copy(aBuf, pStr, 8);
	EXPECT_STREQ(aBuf, "DDNet");
	str_copy(aBuf, pStr, 9);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_copy(aBuf, pStr, 10);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_copy(aBuf, pStr, 11);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_copy(aBuf, pStr, 12);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_copy(aBuf, pStr, 13);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_copy(aBuf, pStr, 14);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_copy(aBuf, pStr, 15);
	EXPECT_STREQ(aBuf, "DDNet最好了");
	str_copy(aBuf, pStr, 16);
	EXPECT_STREQ(aBuf, "DDNet最好了");
	str_copy(aBuf, pStr);
	EXPECT_STREQ(aBuf, "DDNet最好了");
}

TEST(Str, Append)
{
	char aBuf[64];
	aBuf[0] = '\0';
	str_append(aBuf, "DDNet最好了", 7);
	EXPECT_STREQ(aBuf, "DDNet");
	str_append(aBuf, "最", 8);
	EXPECT_STREQ(aBuf, "DDNet");
	str_append(aBuf, "最", 9);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_append(aBuf, "好", 10);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_append(aBuf, "好", 11);
	EXPECT_STREQ(aBuf, "DDNet最");
	str_append(aBuf, "好", 12);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_append(aBuf, "了", 13);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_append(aBuf, "了", 14);
	EXPECT_STREQ(aBuf, "DDNet最好");
	str_append(aBuf, "了", 15);
	EXPECT_STREQ(aBuf, "DDNet最好了");
	str_append(aBuf, "了", 16);
	EXPECT_STREQ(aBuf, "DDNet最好了");
	aBuf[0] = '\0';
	str_append(aBuf, "DDNet最好了");
	EXPECT_STREQ(aBuf, "DDNet最好了");
}

TEST(Str, Utf8Stats)
{
	size_t Size, Count;

	str_utf8_stats("abc", 4, 3, &Size, &Count);
	EXPECT_EQ(Size, 3);
	EXPECT_EQ(Count, 3);

	str_utf8_stats("abc", 2, 3, &Size, &Count);
	EXPECT_EQ(Size, 1);
	EXPECT_EQ(Count, 1);

	str_utf8_stats("", 1, 0, &Size, &Count);
	EXPECT_EQ(Size, 0);
	EXPECT_EQ(Count, 0);

	str_utf8_stats("abcde", 6, 5, &Size, &Count);
	EXPECT_EQ(Size, 5);
	EXPECT_EQ(Count, 5);

	str_utf8_stats("любовь", 13, 6, &Size, &Count);
	EXPECT_EQ(Size, 12);
	EXPECT_EQ(Count, 6);

	str_utf8_stats("abc愛", 7, 4, &Size, &Count);
	EXPECT_EQ(Size, 6);
	EXPECT_EQ(Count, 4);

	str_utf8_stats("abc愛", 6, 4, &Size, &Count);
	EXPECT_EQ(Size, 3);
	EXPECT_EQ(Count, 3);

	str_utf8_stats("любовь", 13, 3, &Size, &Count);
	EXPECT_EQ(Size, 6);
	EXPECT_EQ(Count, 3);
}

TEST(Str, Utf8OffsetBytesToChars)
{
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("", 0), 0);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("", 100), 0);

	EXPECT_EQ(str_utf8_offset_bytes_to_chars("abc", 0), 0);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("abc", 1), 1);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("abc", 2), 2);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("abc", 3), 3);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("abc", 100), 3);

	EXPECT_EQ(str_utf8_offset_bytes_to_chars("любовь", 0), 0);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("любовь", 2), 1);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("любовь", 4), 2);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("любовь", 6), 3);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("любовь", 8), 4);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("любовь", 10), 5);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("любовь", 12), 6);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("любовь", 100), 6);

	EXPECT_EQ(str_utf8_offset_bytes_to_chars("DDNet最好了", 5), 5);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("DDNet最好了", 8), 6);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("DDNet最好了", 11), 7);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("DDNet最好了", 14), 8);
	EXPECT_EQ(str_utf8_offset_bytes_to_chars("DDNet最好了", 100), 8);
}

TEST(Str, Utf8OffsetCharsToBytes)
{
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("", 0), 0);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("", 100), 0);

	EXPECT_EQ(str_utf8_offset_chars_to_bytes("abc", 0), 0);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("abc", 1), 1);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("abc", 2), 2);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("abc", 3), 3);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("abc", 100), 3);

	EXPECT_EQ(str_utf8_offset_chars_to_bytes("любовь", 0), 0);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("любовь", 1), 2);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("любовь", 2), 4);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("любовь", 3), 6);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("любовь", 4), 8);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("любовь", 5), 10);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("любовь", 6), 12);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("любовь", 100), 12);

	EXPECT_EQ(str_utf8_offset_chars_to_bytes("DDNet最好了", 5), 5);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("DDNet最好了", 6), 8);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("DDNet最好了", 7), 11);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("DDNet最好了", 8), 14);
	EXPECT_EQ(str_utf8_offset_chars_to_bytes("DDNet最好了", 100), 14);
}

TEST(Str, Time)
{
	char aBuf[32] = "foobar";

	EXPECT_EQ(str_time(123456, TIME_DAYS, aBuf, 0), -1);
	EXPECT_STREQ(aBuf, "foobar");

	EXPECT_EQ(str_time(123456, TIME_SECS_CENTISECS + 1, aBuf, sizeof(aBuf)), -1);
	EXPECT_STREQ(aBuf, "");

	EXPECT_EQ(str_time(-123456, TIME_MINS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "00.00");

	EXPECT_EQ(str_time(INT64_MAX, TIME_DAYS, aBuf, sizeof(aBuf)), 23);
	EXPECT_STREQ(aBuf, "1067519911673d 00:09:18");

	EXPECT_EQ(str_time(123456, TIME_DAYS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "20:34");
	EXPECT_EQ(str_time(1234567, TIME_DAYS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "03:25:45");
	EXPECT_EQ(str_time(12345678, TIME_DAYS, aBuf, sizeof(aBuf)), 11);
	EXPECT_STREQ(aBuf, "1d 10:17:36");

	EXPECT_EQ(str_time(123456, TIME_HOURS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "20:34");
	EXPECT_EQ(str_time(1234567, TIME_HOURS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "03:25:45");
	EXPECT_EQ(str_time(12345678, TIME_HOURS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "34:17:36");

	EXPECT_EQ(str_time(123456, TIME_MINS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "20:34");
	EXPECT_EQ(str_time(1234567, TIME_MINS, aBuf, sizeof(aBuf)), 6);
	EXPECT_STREQ(aBuf, "205:45");
	EXPECT_EQ(str_time(12345678, TIME_MINS, aBuf, sizeof(aBuf)), 7);
	EXPECT_STREQ(aBuf, "2057:36");

	EXPECT_EQ(str_time(123456, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "20:34.56");
	EXPECT_EQ(str_time(1234567, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf)), 11);
	EXPECT_STREQ(aBuf, "03:25:45.67");
	EXPECT_EQ(str_time(12345678, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf)), 11);
	EXPECT_STREQ(aBuf, "34:17:36.78");

	EXPECT_EQ(str_time(123456, TIME_MINS_CENTISECS, aBuf, sizeof(aBuf)), 8);
	EXPECT_STREQ(aBuf, "20:34.56");
	EXPECT_EQ(str_time(1234567, TIME_MINS_CENTISECS, aBuf, sizeof(aBuf)), 9);
	EXPECT_STREQ(aBuf, "205:45.67");
	EXPECT_EQ(str_time(12345678, TIME_MINS_CENTISECS, aBuf, sizeof(aBuf)), 10);
	EXPECT_STREQ(aBuf, "2057:36.78");

	EXPECT_EQ(str_time(123456, TIME_SECS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "34.56");
	EXPECT_EQ(str_time(1234567, TIME_SECS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "45.67");
	EXPECT_EQ(str_time(12345678, TIME_SECS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "36.78");
}

TEST(Str, TimeFloat)
{
	char aBuf[64];
	EXPECT_EQ(str_time_float(123456.78, TIME_DAYS, aBuf, sizeof(aBuf)), 11);
	EXPECT_STREQ(aBuf, "1d 10:17:36");

	EXPECT_EQ(str_time_float(12.16, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "12.16");

	EXPECT_EQ(str_time_float(22.995, TIME_MINS, aBuf, sizeof(aBuf)), 5);
	EXPECT_STREQ(aBuf, "00:22");
}

TEST(Str, HasCc)
{
	EXPECT_FALSE(str_has_cc(""));
	EXPECT_FALSE(str_has_cc("a"));
	EXPECT_FALSE(str_has_cc("Merhaba dünya!"));

	EXPECT_TRUE(str_has_cc("\n"));
	EXPECT_TRUE(str_has_cc("\r"));
	EXPECT_TRUE(str_has_cc("\t"));
	EXPECT_TRUE(str_has_cc("a\n"));
	EXPECT_TRUE(str_has_cc("a\rb"));
	EXPECT_TRUE(str_has_cc("\tb"));
	EXPECT_TRUE(str_has_cc("\n\n"));
	EXPECT_TRUE(str_has_cc("\x1C"));
	EXPECT_TRUE(str_has_cc("\x1D"));
	EXPECT_TRUE(str_has_cc("\x1E"));
	EXPECT_TRUE(str_has_cc("\x1F"));
}

TEST(Str, SanitizeCc)
{
	char aBuf[64];
	str_copy(aBuf, "");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, "");
	str_copy(aBuf, "a");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, "a");
	str_copy(aBuf, "Merhaba dünya!");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, "Merhaba dünya!");

	str_copy(aBuf, "\n");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, " ");
	str_copy(aBuf, "\r");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, " ");
	str_copy(aBuf, "\t");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, " ");
	str_copy(aBuf, "a\n");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, "a ");
	str_copy(aBuf, "a\rb");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, "a b");
	str_copy(aBuf, "\tb");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, " b");
	str_copy(aBuf, "\n\n");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, "  ");
	str_copy(aBuf, "\x1C");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, " ");
	str_copy(aBuf, "\x1D");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, " ");
	str_copy(aBuf, "\x1E");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, " ");
	str_copy(aBuf, "\x1F");
	str_sanitize_cc(aBuf);
	EXPECT_STREQ(aBuf, " ");
}

TEST(Str, Sanitize)
{
	char aBuf[64];
	str_copy(aBuf, "");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "");
	str_copy(aBuf, "a");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "a");
	str_copy(aBuf, "Merhaba dünya!");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "Merhaba dünya!");
	str_copy(aBuf, "\n");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "\n");
	str_copy(aBuf, "\r");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "\r");
	str_copy(aBuf, "\t");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "\t");
	str_copy(aBuf, "a\n");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "a\n");
	str_copy(aBuf, "a\rb");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "a\rb");
	str_copy(aBuf, "\tb");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "\tb");
	str_copy(aBuf, "\n\n");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, "\n\n");

	str_copy(aBuf, "\x1C");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, " ");
	str_copy(aBuf, "\x1D");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, " ");
	str_copy(aBuf, "\x1E");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, " ");
	str_copy(aBuf, "\x1F");
	str_sanitize(aBuf);
	EXPECT_STREQ(aBuf, " ");
}

TEST(Str, CleanWhitespaces)
{
	char aBuf[64];
	str_copy(aBuf, "aa bb ccc dddd eeeee");
	str_clean_whitespaces(aBuf);
	EXPECT_STREQ(aBuf, "aa bb ccc dddd eeeee");
	str_copy(aBuf, "     ");
	str_clean_whitespaces(aBuf);
	EXPECT_STREQ(aBuf, "");
	str_copy(aBuf, "     aa");
	str_clean_whitespaces(aBuf);
	EXPECT_STREQ(aBuf, "aa");
	str_copy(aBuf, "aa     ");
	str_clean_whitespaces(aBuf);
	EXPECT_STREQ(aBuf, "aa");
	str_copy(aBuf, "  aa   bb    ccc     dddd       eeeee    ");
	str_clean_whitespaces(aBuf);
	EXPECT_STREQ(aBuf, "aa bb ccc dddd eeeee");
}

TEST(Str, SkipToWhitespace)
{
	char aBuf[64];
	str_copy(aBuf, "");
	EXPECT_EQ(str_skip_to_whitespace(aBuf), aBuf);
	EXPECT_EQ(str_skip_to_whitespace_const(aBuf), aBuf);
	str_copy(aBuf, "    a");
	EXPECT_EQ(str_skip_to_whitespace(aBuf), aBuf);
	EXPECT_EQ(str_skip_to_whitespace_const(aBuf), aBuf);
	str_copy(aBuf, "aaaa  b");
	EXPECT_EQ(str_skip_to_whitespace(aBuf), aBuf + 4);
	EXPECT_EQ(str_skip_to_whitespace_const(aBuf), aBuf + 4);
	str_copy(aBuf, "aaaa\n\nb");
	EXPECT_EQ(str_skip_to_whitespace(aBuf), aBuf + 4);
	EXPECT_EQ(str_skip_to_whitespace_const(aBuf), aBuf + 4);
	str_copy(aBuf, "aaaa\r\rb");
	EXPECT_EQ(str_skip_to_whitespace(aBuf), aBuf + 4);
	EXPECT_EQ(str_skip_to_whitespace_const(aBuf), aBuf + 4);
	str_copy(aBuf, "aaaa\t\tb");
	EXPECT_EQ(str_skip_to_whitespace(aBuf), aBuf + 4);
	EXPECT_EQ(str_skip_to_whitespace_const(aBuf), aBuf + 4);
}

TEST(Str, SkipWhitespaces)
{
	char aBuf[64];
	str_copy(aBuf, "");
	EXPECT_EQ(str_skip_whitespaces(aBuf), aBuf);
	EXPECT_EQ(str_skip_whitespaces_const(aBuf), aBuf);
	str_copy(aBuf, "aaaa");
	EXPECT_EQ(str_skip_whitespaces(aBuf), aBuf);
	EXPECT_EQ(str_skip_whitespaces_const(aBuf), aBuf);
	str_copy(aBuf, " \n\r\taaaa");
	EXPECT_EQ(str_skip_whitespaces(aBuf), aBuf + 4);
	EXPECT_EQ(str_skip_whitespaces_const(aBuf), aBuf + 4);
}

TEST(Str, CompFilename)
{
	EXPECT_EQ(str_comp_filenames("a", "a"), 0);
	EXPECT_LT(str_comp_filenames("a", "b"), 0);
	EXPECT_GT(str_comp_filenames("b", "a"), 0);
	EXPECT_EQ(str_comp_filenames("A", "a"), 0);
	EXPECT_LT(str_comp_filenames("A", "b"), 0);
	EXPECT_GT(str_comp_filenames("b", "A"), 0);
	EXPECT_LT(str_comp_filenames("a", "B"), 0);
	EXPECT_GT(str_comp_filenames("B", "a"), 0);
	EXPECT_EQ(str_comp_filenames("1A", "1a"), 0);
	EXPECT_LT(str_comp_filenames("1a", "1B"), 0);
	EXPECT_GT(str_comp_filenames("1B", "1a"), 0);
	EXPECT_LT(str_comp_filenames("1a", "1b"), 0);
	EXPECT_GT(str_comp_filenames("1b", "1a"), 0);
	EXPECT_GT(str_comp_filenames("12a", "1B"), 0);
	EXPECT_LT(str_comp_filenames("1B", "12a"), 0);
	EXPECT_GT(str_comp_filenames("10a", "1B"), 0);
	EXPECT_LT(str_comp_filenames("1B", "10a"), 0);
	EXPECT_GT(str_comp_filenames("10a", "00B"), 0);
	EXPECT_LT(str_comp_filenames("00B", "10a"), 0);
	EXPECT_GT(str_comp_filenames("10a", "09B"), 0);
	EXPECT_LT(str_comp_filenames("09B", "10a"), 0);
	EXPECT_LT(str_comp_filenames("abc", "abcd"), 0);
	EXPECT_GT(str_comp_filenames("abcd", "abc"), 0);
	EXPECT_LT(str_comp_filenames("abc2", "abcd1"), 0);
	EXPECT_GT(str_comp_filenames("abcd1", "abc2"), 0);
	EXPECT_LT(str_comp_filenames("abc50", "abcd"), 0);
	EXPECT_GT(str_comp_filenames("abcd", "abc50"), 0);
	EXPECT_EQ(str_comp_filenames("file0", "file0"), 0);
	EXPECT_LT(str_comp_filenames("file0", "file1"), 0);
	EXPECT_GT(str_comp_filenames("file1", "file0"), 0);
	EXPECT_LT(str_comp_filenames("file1", "file09"), 0);
	EXPECT_GT(str_comp_filenames("file09", "file1"), 0);
	EXPECT_LT(str_comp_filenames("file1", "file009"), 0);
	EXPECT_GT(str_comp_filenames("file009", "file1"), 0);
	EXPECT_GT(str_comp_filenames("file10", "file00"), 0);
	EXPECT_LT(str_comp_filenames("file00", "file10"), 0);
	EXPECT_GT(str_comp_filenames("file10", "file09"), 0);
	EXPECT_LT(str_comp_filenames("file09", "file10"), 0);
	EXPECT_LT(str_comp_filenames("file13", "file37"), 0);
	EXPECT_GT(str_comp_filenames("file37", "file13"), 0);
	EXPECT_LT(str_comp_filenames("file1.ext", "file09.ext"), 0);
	EXPECT_GT(str_comp_filenames("file09.ext", "file1.ext"), 0);
	EXPECT_LT(str_comp_filenames("file1.ext", "file009.ext"), 0);
	EXPECT_GT(str_comp_filenames("file009.ext", "file1.ext"), 0);
	EXPECT_EQ(str_comp_filenames("file0.ext", "file0.ext"), 0);
	EXPECT_LT(str_comp_filenames("file13.ext", "file37.ext"), 0);
	EXPECT_GT(str_comp_filenames("file37.ext", "file13.ext"), 0);
	EXPECT_LT(str_comp_filenames("FILE13.EXT", "file37.ext"), 0);
	EXPECT_GT(str_comp_filenames("file37.ext", "FILE13.EXT"), 0);
	EXPECT_GT(str_comp_filenames("file10.ext", "file00.ext"), 0);
	EXPECT_LT(str_comp_filenames("file00.ext", "file10.ext"), 0);
	EXPECT_GT(str_comp_filenames("file10.ext", "file09.ext"), 0);
	EXPECT_LT(str_comp_filenames("file09.ext", "file10.ext"), 0);
	EXPECT_LT(str_comp_filenames("file42", "file1337"), 0);
	EXPECT_GT(str_comp_filenames("file1337", "file42"), 0);
	EXPECT_LT(str_comp_filenames("file42.ext", "file1337.ext"), 0);
	EXPECT_GT(str_comp_filenames("file1337.ext", "file42.ext"), 0);
	EXPECT_GT(str_comp_filenames("file4414520", "file2055"), 0);
	EXPECT_LT(str_comp_filenames("file4414520", "file205523151812419"), 0);
}

TEST(Str, RightChar)
{
	const char *pStr = "a bb ccc dddd       eeeee";
	EXPECT_EQ(str_rchr(pStr, 'a'), pStr);
	EXPECT_EQ(str_rchr(pStr, 'b'), pStr + 3);
	EXPECT_EQ(str_rchr(pStr, 'c'), pStr + 7);
	EXPECT_EQ(str_rchr(pStr, 'd'), pStr + 12);
	EXPECT_EQ(str_rchr(pStr, ' '), pStr + 19);
	EXPECT_EQ(str_rchr(pStr, 'e'), pStr + 24);
	EXPECT_EQ(str_rchr(pStr, '\0'), pStr + str_length(pStr));
	EXPECT_EQ(str_rchr(pStr, 'y'), nullptr);
}

TEST(Str, CountChar)
{
	const char *pStr = "a bb ccc dddd       eeeee";
	EXPECT_EQ(str_countchr(pStr, 'a'), 1);
	EXPECT_EQ(str_countchr(pStr, 'b'), 2);
	EXPECT_EQ(str_countchr(pStr, 'c'), 3);
	EXPECT_EQ(str_countchr(pStr, 'd'), 4);
	EXPECT_EQ(str_countchr(pStr, 'e'), 5);
	EXPECT_EQ(str_countchr(pStr, ' '), 10);
	EXPECT_EQ(str_countchr(pStr, '\0'), 0);
	EXPECT_EQ(str_countchr(pStr, 'y'), 0);
}

TEST(Str, StrToInts)
{
	int aInts[8];

	StrToInts(aInts, 1, "a");
	EXPECT_EQ(aInts[0], 0xE1808000);

	StrToInts(aInts, 1, "ab");
	EXPECT_EQ(aInts[0], 0xE1E28000);

	StrToInts(aInts, 1, "abc");
	EXPECT_EQ(aInts[0], 0xE1E2E300);

	StrToInts(aInts, 2, "abcd");
	EXPECT_EQ(aInts[0], 0xE1E2E3E4);
	EXPECT_EQ(aInts[1], 0x80808000);

	StrToInts(aInts, 2, "abcde");
	EXPECT_EQ(aInts[0], 0xE1E2E3E4);
	EXPECT_EQ(aInts[1], 0xE5808000);

	StrToInts(aInts, 2, "abcdef");
	EXPECT_EQ(aInts[0], 0xE1E2E3E4);
	EXPECT_EQ(aInts[1], 0xE5E68000);

	StrToInts(aInts, 2, "abcdefg");
	EXPECT_EQ(aInts[0], 0xE1E2E3E4);
	EXPECT_EQ(aInts[1], 0xE5E6E700);

	StrToInts(aInts, 2, "öüä");
	EXPECT_EQ(aInts[0], 0x4336433C);
	EXPECT_EQ(aInts[1], 0x43248000);

	StrToInts(aInts, 3, "aβい🐘");
	EXPECT_EQ(aInts[0], 0xE14E3263);
	EXPECT_EQ(aInts[1], 0x0104701F);
	EXPECT_EQ(aInts[2], 0x10188000);

	// long padding
	StrToInts(aInts, 4, "abc");
	EXPECT_EQ(aInts[0], 0xE1E2E380);
	EXPECT_EQ(aInts[1], 0x80808080);
	EXPECT_EQ(aInts[2], 0x80808080);
	EXPECT_EQ(aInts[3], 0x80808000);
}

TEST(Str, IntsToStr)
{
	int aInts[8];
	char aStr[sizeof(aInts)];

	aInts[0] = 0xE1808000;
	EXPECT_TRUE(IntsToStr(aInts, 1, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "a");

	aInts[0] = 0xE1E28000;
	EXPECT_TRUE(IntsToStr(aInts, 1, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "ab");

	aInts[0] = 0xE1E2E300;
	EXPECT_TRUE(IntsToStr(aInts, 1, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "abc");

	aInts[0] = 0xE1E2E3E4;
	aInts[1] = 0x80808000;
	EXPECT_TRUE(IntsToStr(aInts, 2, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "abcd");

	aInts[0] = 0xE1E2E3E4;
	aInts[1] = 0xE5808000;
	EXPECT_TRUE(IntsToStr(aInts, 2, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "abcde");

	aInts[0] = 0xE1E2E3E4;
	aInts[1] = 0xE5E68000;
	EXPECT_TRUE(IntsToStr(aInts, 2, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "abcdef");

	aInts[0] = 0xE1E2E3E4;
	aInts[1] = 0xE5E6E700;
	EXPECT_TRUE(IntsToStr(aInts, 2, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "abcdefg");

	aInts[0] = 0x4336433C;
	aInts[1] = 0x43248000;
	EXPECT_TRUE(IntsToStr(aInts, 2, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "öüä");

	aInts[0] = 0xE14E3263;
	aInts[1] = 0x0104701F;
	aInts[2] = 0x10188000;
	EXPECT_TRUE(IntsToStr(aInts, 3, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "aβい🐘");

	// long padding
	aInts[0] = 0xE1E2E380;
	aInts[1] = 0x80808080;
	aInts[2] = 0x80808080;
	aInts[3] = 0x80808000;
	EXPECT_TRUE(IntsToStr(aInts, 4, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "abc");

	// early null character (0x80)
	aInts[0] = 0xE1E2E380;
	aInts[1] = 0xE1E2E3E4;
	aInts[2] = 0xE1E2E3E4;
	aInts[3] = 0xE1E2E300;
	EXPECT_TRUE(IntsToStr(aInts, 4, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "abc");

	// invalid UTF-8
	aInts[0] = 0xE17FE200;
	EXPECT_FALSE(IntsToStr(aInts, 1, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "");

	// invalid UTF-8 (0x00 in string data, which is translated to '\x80')
	aInts[0] = 0xE100E200;
	EXPECT_FALSE(IntsToStr(aInts, 1, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "");

	// invalid UTF-8
	aInts[0] = 0xE1E2E36F;
	aInts[1] = 0x3F40E4E5;
	aInts[2] = 0xE67FE700;
	EXPECT_FALSE(IntsToStr(aInts, 3, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "");

	// invalid UTF-8 and missing null-terminator
	aInts[0] = 0x7F7F7F7F;
	EXPECT_FALSE(IntsToStr(aInts, 1, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "");

	// missing null-terminator at the end is ignored
	aInts[0] = 0xE1E2E3E4;
	EXPECT_TRUE(IntsToStr(aInts, 1, aStr, std::size(aStr)));
	EXPECT_STREQ(aStr, "abc");

	// basic fuzzing: no input integer should result in invalid UTF-8 in the string
	for(int i = 0; i <= 0xFFFFFF; i += 7) // checking all values takes a bit too long
	{
		mem_zero(aStr, std::size(aStr));
		aInts[0] = 0xE1 << 24 | i;
		aInts[1] = i << 8 | 0xE2;
		aInts[2] = 0xE3 << 24 | i;
		const bool ConversionResult = IntsToStr(aInts, 3, aStr, std::size(aStr));
		// ensure null-termination before calling str_utf8_check
		ASSERT_TRUE(mem_has_null(aStr, std::size(aStr)));
		ASSERT_TRUE(str_utf8_check(aStr));
		ASSERT_TRUE(ConversionResult || aStr[0] == '\0');
	}
}

#if defined(CONF_FAMILY_WINDOWS)
TEST(Str, WindowsUtf8WideConversion)
{
	const char *apUtf8Strings[] = {
		"",
		"abc",
		"a bb ccc dddd       eeeee",
		"öüä",
		"привет Наташа",
		"ąçęįǫų",
		"DDNet最好了",
		"aβい🐘"};
	const wchar_t *apWideStrings[] = {
		L"",
		L"abc",
		L"a bb ccc dddd       eeeee",
		L"öüä",
		L"привет Наташа",
		L"ąçęįǫų",
		L"DDNet最好了",
		L"aβい🐘"};
	static_assert(std::size(apUtf8Strings) == std::size(apWideStrings));
	for(size_t i = 0; i < std::size(apUtf8Strings); i++)
	{
		const std::optional<std::string> ConvertedUtf8 = windows_wide_to_utf8(apWideStrings[i]);
		const std::wstring ConvertedWide = windows_utf8_to_wide(apUtf8Strings[i]);
		ASSERT_TRUE(ConvertedUtf8.has_value());
		EXPECT_STREQ(ConvertedUtf8.value().c_str(), apUtf8Strings[i]);
		EXPECT_STREQ(ConvertedWide.c_str(), apWideStrings[i]);
	}
}
#endif
