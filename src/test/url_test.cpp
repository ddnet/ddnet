#include <base/url.h>

#include <gtest/gtest.h>

#include <string>

TEST(Url, FindCommonTlds)
{
	const auto vUrls = url_find_all("example.com EXAMPLE.ORG sub.example.net example.dev example.computer autoexec.cfg");
	ASSERT_EQ(vUrls.size(), 3u);
	EXPECT_STREQ(vUrls[0].m_aText, "example.com");
	EXPECT_STREQ(vUrls[1].m_aText, "EXAMPLE.ORG");
	EXPECT_STREQ(vUrls[2].m_aText, "sub.example.net");
	EXPECT_STREQ(vUrls[2].m_aDomainName, "example.net");
	for(const auto &Url : vUrls)
		EXPECT_FALSE(Url.m_HasScheme);
}

TEST(Url, FindExplicitLinks)
{
	const auto vUrls = url_find_all("(HTTPS://sub.example.dev:8080/path?q=a:b&x=1#part), http://example.info/ WWW.example.io.");
	ASSERT_EQ(vUrls.size(), 3u);
	EXPECT_STREQ(vUrls[0].m_aText, "HTTPS://sub.example.dev:8080/path?q=a:b&x=1#part");
	EXPECT_STREQ(vUrls[0].m_aDomainName, "example.dev");
	EXPECT_TRUE(vUrls[0].m_HasScheme);
	EXPECT_STREQ(vUrls[1].m_aText, "http://example.info/");
	EXPECT_TRUE(vUrls[1].m_HasScheme);
	EXPECT_STREQ(vUrls[2].m_aText, "WWW.example.io");
	EXPECT_FALSE(vUrls[2].m_HasScheme);
}

TEST(Url, FindPunctuationAndOffsets)
{
	const auto vUrls = url_find_all("\xC3\xA9 example.com, https://ddnet.org/page.? next");
	ASSERT_EQ(vUrls.size(), 2u);
	EXPECT_EQ(vUrls[0].m_OffsetBytes, 3u);
	EXPECT_EQ(vUrls[0].m_OffsetChar, 2u);
	EXPECT_EQ(vUrls[0].m_Length, 11u);
	EXPECT_STREQ(vUrls[0].m_aText, "example.com");
	EXPECT_EQ(vUrls[1].m_OffsetBytes, 16u);
	EXPECT_EQ(vUrls[1].m_OffsetChar, 15u);
	EXPECT_EQ(vUrls[1].m_Length, 22u);
	EXPECT_STREQ(vUrls[1].m_aText, "https://ddnet.org/page");
}

TEST(Url, FindRejectsInvalidHosts)
{
	for(const char *pText : {
		    "user@example.com", "my_site.com", "-example.com", "example-.com",
		    "example..com", "example.com_foo", "example.com1", "example.com%20.evil",
		    "example.com@evil.org", "https://example.com@evil.org", "ftp://example.com",
		    "https://localhost", "https://127.0.0.1", "https://[::1]", "example.c",
		    "https://example.com:123456/path", "https://example.com:80abc",
		    "\xC3\xA9xample.com", "example.com\xC3\xA9"})
	{
		SCOPED_TRACE(pText);
		EXPECT_TRUE(url_find_all(pText).empty());
	}
}

TEST(Url, FindBoundedInput)
{
	const char aText[] = {'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm'};
	const auto vUrls = url_find_all(aText, sizeof(aText));
	ASSERT_EQ(vUrls.size(), 1u);
	EXPECT_STREQ(vUrls[0].m_aText, "example.com");
	EXPECT_STREQ(vUrls[0].m_aDomainName, "example.com");
	EXPECT_TRUE(url_find_all(aText, 7).empty());
	EXPECT_TRUE(url_find_all(nullptr).empty());
	EXPECT_TRUE(url_find_all(nullptr, 10).empty());
	EXPECT_TRUE(url_find_all(std::string_view{}).empty());
	const char aTruncatedScheme[] = {'h', 't', 't', 'p', 's'};
	EXPECT_TRUE(url_find_all(aTruncatedScheme, sizeof(aTruncatedScheme)).empty());
}

TEST(Url, FindLengthLimits)
{
	EXPECT_EQ(url_find_all(std::string(63, 'a') + ".com").size(), 1u);
	EXPECT_TRUE(url_find_all(std::string(64, 'a') + ".com").empty());
	EXPECT_EQ(url_find_all("https://example." + std::string(63, 'a')).size(), 1u);
	EXPECT_TRUE(url_find_all("https://example." + std::string(64, 'a')).empty());
	std::string Url = "https://example.com/";
	Url.resize(URL_MAX_LENGTH - 1, 'a');
	const auto vUrls = url_find_all(Url);
	ASSERT_EQ(vUrls.size(), 1u);
	EXPECT_EQ(vUrls[0].m_Length, URL_MAX_LENGTH - 1);
	EXPECT_STREQ(vUrls[0].m_aText, Url.c_str());
	Url += 'a';
	EXPECT_TRUE(url_find_all(Url).empty());
	Url += " example.org";
	const auto vAfterLongUrl = url_find_all(Url);
	ASSERT_EQ(vAfterLongUrl.size(), 1u);
	EXPECT_STREQ(vAfterLongUrl[0].m_aText, "example.org");
}

TEST(Url, FindResultLimit)
{
	std::string Text;
	for(std::size_t i = 0; i < URL_MAX_RESULTS + 1; ++i)
		Text += "example.com ";
	const auto vUrls = url_find_all(Text);
	ASSERT_EQ(vUrls.size(), URL_MAX_RESULTS);
	EXPECT_EQ(vUrls.back().m_OffsetBytes, (URL_MAX_RESULTS - 1) * 12);
}

TEST(Url, PortAndSentencePunctuation)
{
	const auto vUrls = url_find_all("example.com:80. example.org:443/path. example.net:notes");
	ASSERT_EQ(vUrls.size(), 3u);
	EXPECT_STREQ(vUrls[0].m_aText, "example.com:80");
	EXPECT_STREQ(vUrls[1].m_aText, "example.org:443/path");
	EXPECT_STREQ(vUrls[2].m_aText, "example.net");
}

TEST(Url, PathQueryAndFragment)
{
	const auto vUrls = url_find_all("example.com/path?q=x:y,example.org#part example.net/?next=https://ddnet.org/");
	ASSERT_EQ(vUrls.size(), 3u);
	EXPECT_STREQ(vUrls[0].m_aText, "example.com/path?q=x:y");
	EXPECT_STREQ(vUrls[1].m_aText, "example.org#part");
	EXPECT_STREQ(vUrls[2].m_aText, "example.net/?next=https://ddnet.org/");
}

TEST(Url, MultipleUtf8Offsets)
{
	const std::string Prefix(10000, ' ');
	const auto vUrls = url_find_all(Prefix + "\xC3\xA9 example.com \xF0\x9F\x98\x80 example.org");
	ASSERT_EQ(vUrls.size(), 2u);
	EXPECT_EQ(vUrls[0].m_OffsetBytes, Prefix.size() + 3);
	EXPECT_EQ(vUrls[0].m_OffsetChar, Prefix.size() + 2);
	EXPECT_EQ(vUrls[1].m_OffsetBytes, Prefix.size() + 20);
	EXPECT_EQ(vUrls[1].m_OffsetChar, Prefix.size() + 16);
}

TEST(Url, RejectPartialHosts)
{
	// Unlike the regex, do not turn a malformed authority into a trusted prefix.
	for(const char *pText : {"www.example..com", "https://example.com.123", "https://example.com.-bad", "https://example.com@evil.org", "https://example.com:123456"})
	{
		SCOPED_TRACE(pText);
		EXPECT_TRUE(url_find_all(pText).empty());
	}
}

TEST(Url, DomainAllowlist)
{
	EXPECT_TRUE(url_is_valid_domain_name("sub.example.dev"));
	EXPECT_FALSE(url_is_valid_domain_name("example..com"));
	EXPECT_TRUE(url_check_domain_name("https://Sub.Example.COM:443/path", "example.com"));
	EXPECT_TRUE(url_check_domain_name("https://example.dev/", "example.dev"));
	EXPECT_FALSE(url_check_domain_name("https://notexample.com", "example.com"));
	EXPECT_FALSE(url_check_domain_name("https://example.com.evil.org", "example.com"));
	EXPECT_FALSE(url_check_domain_name("https://example.com@evil.org", "example.com"));
	EXPECT_FALSE(url_check_domain_name("javascript://example.com", "example.com"));
}
