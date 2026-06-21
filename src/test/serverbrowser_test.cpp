#include "test.h"

#include <base/net.h>
#include <base/str.h>

#include <engine/client/serverbrowser_filter.h>
#include <engine/client/serverbrowser_ping_cache.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <gtest/gtest.h>

#include <cstring>
#include <memory>

TEST(ServerBrowser, PingCache)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;

	auto pConsole = CreateConsole(CFGFLAG_CLIENT);
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr) << "Error creating test storage";
	auto pPingCache = std::unique_ptr<IServerBrowserPingCache>(CreateServerBrowserPingCache(pConsole.get(), pStorage.get()));

	NETADDR Localhost4, Localhost6, OtherLocalhost4, OtherLocalhost6;
	ASSERT_FALSE(net_addr_from_str(&Localhost4, "127.0.0.1:8303"));
	ASSERT_FALSE(net_addr_from_str(&Localhost6, "[::1]:8304"));
	ASSERT_FALSE(net_addr_from_str(&OtherLocalhost4, "127.0.0.1:8305"));
	ASSERT_FALSE(net_addr_from_str(&OtherLocalhost6, "[::1]:8306"));
	EXPECT_LT(net_addr_comp(&Localhost4, &Localhost6), 0);
	NETADDR aLocalhostBoth[2] = {Localhost4, Localhost6};

	EXPECT_EQ(pPingCache->NumEntries(), 0);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	pPingCache->Load();

	EXPECT_EQ(pPingCache->NumEntries(), 0);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	// Newer pings overwrite older.
	pPingCache->CachePing(Localhost4, 123);
	pPingCache->CachePing(Localhost4, 234);
	pPingCache->CachePing(Localhost4, 345);
	pPingCache->CachePing(Localhost4, 456);
	pPingCache->CachePing(Localhost4, 567);
	pPingCache->CachePing(Localhost4, 678);
	pPingCache->CachePing(Localhost4, 789);
	pPingCache->CachePing(Localhost4, 890);
	pPingCache->CachePing(Localhost4, 901);
	pPingCache->CachePing(Localhost4, 135);
	pPingCache->CachePing(Localhost4, 246);
	pPingCache->CachePing(Localhost4, 357);
	pPingCache->CachePing(Localhost4, 468);
	pPingCache->CachePing(Localhost4, 579);
	pPingCache->CachePing(Localhost4, 680);
	pPingCache->CachePing(Localhost4, 791);
	pPingCache->CachePing(Localhost4, 802);
	pPingCache->CachePing(Localhost4, 913);

	EXPECT_EQ(pPingCache->NumEntries(), 1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 913);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 913);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 913);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	pPingCache->CachePing(Localhost4, 234);
	pPingCache->CachePing(Localhost6, 345);
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 234);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 234);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 234);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	// Port doesn't matter for overwriting.
	pPingCache->CachePing(Localhost4, 1337);
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 345);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	pPingCache.reset(CreateServerBrowserPingCache(pConsole.get(), pStorage.get()));

	// Persistence.
	pPingCache->Load();
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 345);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);
}

using serverbrowser_filter::EFilterField;
using serverbrowser_filter::NextWhitespaceToken;
using serverbrowser_filter::ParseFilterToken;
using serverbrowser_filter::SFilterToken;
using serverbrowser_filter::TokenMatches;

namespace
{

	// ParseFilterToken mutates its input, so wrap the common pattern of copying
	// a literal into a fresh writable buffer before parsing.
	SFilterToken ParseCopy(char *pBuffer, size_t BufferSize, const char *pInput)
	{
		str_copy(pBuffer, pInput, BufferSize);
		return ParseFilterToken(pBuffer);
	}

} // namespace

TEST(ServerBrowserFilter, ParseTokenPlain)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "hello");
	EXPECT_EQ(Token.m_Field, EFilterField::ALL);
	EXPECT_STREQ(Token.m_pValue, "hello");
	EXPECT_FALSE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenEmpty)
{
	char aBuf[64] = "";
	const SFilterToken Token = ParseFilterToken(aBuf);
	EXPECT_EQ(Token.m_Field, EFilterField::ALL);
	EXPECT_STREQ(Token.m_pValue, "");
	EXPECT_FALSE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenKeys)
{
	struct
	{
		const char *m_pInput;
		EFilterField m_ExpectedField;
		const char *m_pExpectedValue;
	} const s_aCases[] = {
		{"map:foo", EFilterField::MAP, "foo"},
		{"type:dm", EFilterField::TYPE, "dm"},
		{"addr:127.0.0.1", EFilterField::ADDR, "127.0.0.1"},
		{"name:bob", EFilterField::NAME, "bob"},
		{"player:alice", EFilterField::PLAYER, "alice"},
	};
	for(const auto &Case : s_aCases)
	{
		char aBuf[64];
		const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), Case.m_pInput);
		EXPECT_EQ(Token.m_Field, Case.m_ExpectedField) << Case.m_pInput;
		EXPECT_STREQ(Token.m_pValue, Case.m_pExpectedValue) << Case.m_pInput;
		EXPECT_FALSE(Token.m_Exact) << Case.m_pInput;
	}
}

TEST(ServerBrowserFilter, ParseTokenKeyCaseInsensitive)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "MAP:foo");
	EXPECT_EQ(Token.m_Field, EFilterField::MAP);
	EXPECT_STREQ(Token.m_pValue, "foo");
}

TEST(ServerBrowserFilter, ParseTokenKeyEmptyValue)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "map:");
	EXPECT_EQ(Token.m_Field, EFilterField::MAP);
	EXPECT_STREQ(Token.m_pValue, "");
	EXPECT_FALSE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenUnknownKeyIsLiteral)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "foo:bar");
	EXPECT_EQ(Token.m_Field, EFilterField::ALL);
	EXPECT_STREQ(Token.m_pValue, "foo:bar");
}

TEST(ServerBrowserFilter, ParseTokenColonOnly)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), ":");
	EXPECT_EQ(Token.m_Field, EFilterField::ALL);
	EXPECT_STREQ(Token.m_pValue, ":");
}

TEST(ServerBrowserFilter, ParseTokenQuoted)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "\"hello\"");
	EXPECT_EQ(Token.m_Field, EFilterField::ALL);
	EXPECT_STREQ(Token.m_pValue, "hello");
	EXPECT_TRUE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenEmptyQuoted)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "\"\"");
	EXPECT_EQ(Token.m_Field, EFilterField::ALL);
	EXPECT_STREQ(Token.m_pValue, "");
	EXPECT_TRUE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenSingleQuote)
{
	// A single `"` is length 1 — too short to be a quoted exact token.
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "\"");
	EXPECT_EQ(Token.m_Field, EFilterField::ALL);
	EXPECT_STREQ(Token.m_pValue, "\"");
	EXPECT_FALSE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenOpenQuoteOnly)
{
	// Leading quote but no trailing quote must NOT be treated as exact.
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "\"hello");
	EXPECT_EQ(Token.m_Field, EFilterField::ALL);
	EXPECT_STREQ(Token.m_pValue, "\"hello");
	EXPECT_FALSE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenCloseQuoteOnly)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "hello\"");
	EXPECT_EQ(Token.m_Field, EFilterField::ALL);
	EXPECT_STREQ(Token.m_pValue, "hello\"");
	EXPECT_FALSE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenKeyQuoted)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "name:\"John Doe\"");
	EXPECT_EQ(Token.m_Field, EFilterField::NAME);
	EXPECT_STREQ(Token.m_pValue, "John Doe");
	EXPECT_TRUE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenKeyEmptyQuoted)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "map:\"\"");
	EXPECT_EQ(Token.m_Field, EFilterField::MAP);
	EXPECT_STREQ(Token.m_pValue, "");
	EXPECT_TRUE(Token.m_Exact);
}

TEST(ServerBrowserFilter, ParseTokenKeyPrefixOnly)
{
	// Exactly `map:` — empty value, no quotes. The quote check requires
	// Len >= 2, so this exercises the length guard.
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "map:");
	EXPECT_EQ(Token.m_Field, EFilterField::MAP);
	EXPECT_STREQ(Token.m_pValue, "");
}

TEST(ServerBrowserFilter, ParseTokenUtf8Value)
{
	char aBuf[64];
	const SFilterToken Token = ParseCopy(aBuf, sizeof(aBuf), "name:\xf0\x9f\x98\x80");
	EXPECT_EQ(Token.m_Field, EFilterField::NAME);
	EXPECT_STREQ(Token.m_pValue, "\xf0\x9f\x98\x80");
}

TEST(ServerBrowserFilter, NextTokenEmpty)
{
	char aBuf[64];
	EXPECT_EQ(NextWhitespaceToken("", aBuf, sizeof(aBuf)), nullptr);
}

TEST(ServerBrowserFilter, NextTokenWhitespaceOnly)
{
	char aBuf[64];
	EXPECT_EQ(NextWhitespaceToken("   \t  ", aBuf, sizeof(aBuf)), nullptr);
}

TEST(ServerBrowserFilter, NextTokenSingle)
{
	char aBuf[64];
	const char *pRest = NextWhitespaceToken("foo", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "foo");
	EXPECT_STREQ(pRest, "");
	EXPECT_EQ(NextWhitespaceToken(pRest, aBuf, sizeof(aBuf)), nullptr);
}

TEST(ServerBrowserFilter, NextTokenMultiple)
{
	char aBuf[64];
	const char *pRest = NextWhitespaceToken("  foo  bar\tbaz   ", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "foo");

	pRest = NextWhitespaceToken(pRest, aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "bar");

	pRest = NextWhitespaceToken(pRest, aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "baz");

	EXPECT_EQ(NextWhitespaceToken(pRest, aBuf, sizeof(aBuf)), nullptr);
}

TEST(ServerBrowserFilter, NextTokenQuotedSpaces)
{
	char aBuf[64];
	const char *pRest = NextWhitespaceToken("\"hello world\" end", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "\"hello world\"");

	pRest = NextWhitespaceToken(pRest, aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "end");

	EXPECT_EQ(NextWhitespaceToken(pRest, aBuf, sizeof(aBuf)), nullptr);
}

TEST(ServerBrowserFilter, NextTokenUnclosedQuote)
{
	// Unclosed quote consumes to end-of-string without overrunning.
	char aBuf[64];
	const char *pRest = NextWhitespaceToken("\"unterminated", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "\"unterminated");
	EXPECT_EQ(*pRest, '\0');
	EXPECT_EQ(NextWhitespaceToken(pRest, aBuf, sizeof(aBuf)), nullptr);
}

TEST(ServerBrowserFilter, NextTokenQuoteMidToken)
{
	// `foo"bar baz"qux` is a single token: the embedded quote toggles the
	// "in quotes" state, so the interior space does not end the token.
	char aBuf[64];
	const char *pRest = NextWhitespaceToken("foo\"bar baz\"qux after", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "foo\"bar baz\"qux");

	pRest = NextWhitespaceToken(pRest, aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "after");
}

TEST(ServerBrowserFilter, NextTokenAdjacentTokens)
{
	// `a;b c` — semicolons are not treated as token separators here (the
	// caller splits on `;` before calling NextWhitespaceToken). The first
	// token spans `a;b`, the second is `c`.
	char aBuf[64];
	const char *pRest = NextWhitespaceToken("a;b c", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "a;b");

	pRest = NextWhitespaceToken(pRest, aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "c");
}

TEST(ServerBrowserFilter, NextTokenBufferTruncation)
{
	// A 5-byte buffer holds 4 chars + NUL. The token must be truncated
	// cleanly and the function must return a non-null cursor so subsequent
	// calls can continue.
	char aBuf[5];
	const char *pRest = NextWhitespaceToken("abcdefg hi", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_EQ(aBuf[sizeof(aBuf) - 1], '\0');
	EXPECT_LE(std::strlen(aBuf), sizeof(aBuf) - 1);
	// After truncation the parser stops writing but the cursor still points
	// somewhere inside the remaining string. Drive it to completion to make
	// sure no undefined state leaks out.
	while(pRest != nullptr)
		pRest = NextWhitespaceToken(pRest, aBuf, sizeof(aBuf));
}

TEST(ServerBrowserFilter, NextTokenTinyBuffer)
{
	// Two-byte buffer holds one character + NUL.
	char aBuf[2];
	const char *pRest = NextWhitespaceToken("abc de", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_EQ(aBuf[1], '\0');
	while(pRest != nullptr)
		pRest = NextWhitespaceToken(pRest, aBuf, sizeof(aBuf));
}

TEST(ServerBrowserFilter, NextTokenUtf8)
{
	// Multi-byte UTF-8 must not be split mid-codepoint: the inner loop
	// copies one full codepoint at a time while the output has room.
	char aBuf[64];
	const char *pRest = NextWhitespaceToken("\xe4\xb8\x96\xe7\x95\x8c hello", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "\xe4\xb8\x96\xe7\x95\x8c");

	pRest = NextWhitespaceToken(pRest, aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "hello");
}

TEST(ServerBrowserFilter, NextTokenLoneQuoteToken)
{
	// A bare `"` opens quote mode, then end-of-string terminates the loop.
	// Should not infinite-loop or read past the NUL.
	char aBuf[64];
	const char *pRest = NextWhitespaceToken("\"", aBuf, sizeof(aBuf));
	ASSERT_NE(pRest, nullptr);
	EXPECT_STREQ(aBuf, "\"");
	EXPECT_EQ(*pRest, '\0');
}

TEST(ServerBrowserFilter, TokenMatchesSubstringCaseInsensitive)
{
	char aBuf[] = "foo";
	SFilterToken Token = ParseFilterToken(aBuf);
	EXPECT_TRUE(TokenMatches("FooBar", Token));
	EXPECT_TRUE(TokenMatches("foo", Token));
	EXPECT_FALSE(TokenMatches("bar", Token));
}

TEST(ServerBrowserFilter, TokenMatchesExact)
{
	char aBuf[] = "\"foo\"";
	SFilterToken Token = ParseFilterToken(aBuf);
	ASSERT_TRUE(Token.m_Exact);
	EXPECT_TRUE(TokenMatches("foo", Token));
	EXPECT_FALSE(TokenMatches("Foo", Token));
	EXPECT_FALSE(TokenMatches("foobar", Token));
}

TEST(ServerBrowserFilter, TokenMatchesEmptySubstring)
{
	// An empty substring matches any non-empty field (str_utf8_find_nocase
	// returns nullptr when the haystack is also empty, but the important
	// property for this test is that it doesn't crash or read out of
	// bounds on either input being empty).
	char aBuf[] = "";
	SFilterToken Token = ParseFilterToken(aBuf);
	ASSERT_FALSE(Token.m_Exact);
	EXPECT_TRUE(TokenMatches("anything", Token));
	(void)TokenMatches("", Token);
}

TEST(ServerBrowserFilter, TokenMatchesEmptyExact)
{
	char aBuf[] = "\"\"";
	SFilterToken Token = ParseFilterToken(aBuf);
	ASSERT_TRUE(Token.m_Exact);
	EXPECT_TRUE(TokenMatches("", Token));
	EXPECT_FALSE(TokenMatches("x", Token));
}

TEST(ServerBrowserFilter, FuzzNoCrash)
{
	// Drive every entry through the full parser pipeline: walk the string
	// with NextWhitespaceToken, then hand each emitted token to
	// ParseFilterToken + TokenMatches. The assertion is simply "no crash,
	// no undefined behavior" — we do not check outputs here.
	const char *s_apInputs[] = {
		"",
		" ",
		"\t\n",
		":",
		"::",
		":::::",
		"\"",
		"\"\"",
		"\"\"\"",
		"\"\"\"\"",
		"map:",
		"map:\"",
		"map:\"\"",
		"map:\"unterminated",
		"name:\"\"\"\"",
		"addr::",
		"player:player:",
		"foo:bar:baz",
		"MaP:Foo TyPe:DM aDdR:1.2.3.4",
		"name:\"a b c\" player:\"x\"",
		"\"\"\"\"\"\"\"\"\"\"",
		"         ",
		"a  b  c  d  e",
		"\xff\xfe\xfd",
		"\xc3\x28",
		"foo\"bar\"baz",
		"\"foo\" \"bar\" \"baz\"",
		"name:map:type:foo",
		"name:\"\xf0\x9f\x98\x80\"",
	};

	for(const char *pInput : s_apInputs)
	{
		const char *pCursor = pInput;
		char aToken[128];
		int Iterations = 0;
		while((pCursor = NextWhitespaceToken(pCursor, aToken, sizeof(aToken))))
		{
			const SFilterToken Token = ParseFilterToken(aToken);
			(void)TokenMatches("sample field", Token);
			(void)TokenMatches("", Token);
			ASSERT_LT(++Iterations, 1024) << "Runaway loop for input: " << pInput;
		}
	}
}
