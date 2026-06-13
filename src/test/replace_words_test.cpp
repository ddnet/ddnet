#include <base/str.h>

#include <game/client/components/censor.h>

#include <gtest/gtest.h>

static void Censor(const char *pInput, const char *pExpectedOutput, const std::vector<std::string> &vWords)
{
	char aBuf[512];
	str_copy(aBuf, pInput);
	CensorReplaceWords(aBuf, vWords, '*');
	EXPECT_STREQ(aBuf, pExpectedOutput);
}

TEST(ReplaceWords, Simple)
{
	Censor("hello", "*****", {"hello"});
	Censor("hello world", "hello *****", {"world"});
	Censor("foo bar baz", "*** *** ***", {"baz", "bar", "foo"});
}

TEST(ReplaceWords, DISABLED_MissingBoundaries)
{
	// TODO: not working yet
	Censor("foofoo", "******", {"foo"});
	Censor("you little foo!", "you little ***!", {"foo"});
	Censor("bаr", "***", {"bar"}); // cyrillic a in input and latin a in censor
}

TEST(ReplaceWords, DISABLED_Cyrillic)
{
	// TODO: not working yet
	// cyrillic a in input and latin a in censor
	Censor("bаr", "***", {"bar"});
}

TEST(ReplaceWords, DISABLED_Spread)
{
	// TODO: not working yet
	Censor("h,e,l,l,o", "*********", {"hello"});
	Censor("hell o", "******", {"hello"});
	Censor("h e l l o", "*********", {"hello"});
}
