#include <base/hash_ctxt.h>
#include <base/str.h>

#include <gtest/gtest.h>

template<size_t BufferSize = SHA256_MAXSTRSIZE>
static void ExpectSha256(SHA256_DIGEST Actual, const char *pWanted)
{
	char aActual[BufferSize];
	sha256_str(Actual, aActual, sizeof(aActual));
	EXPECT_STREQ(aActual, pWanted);
}

constexpr static const char QUICK_BROWN_FOX[] = "The quick brown fox jumps over the lazy dog.";

TEST(Hash, Sha256)
{
	// https://en.wikipedia.org/w/index.php?title=SHA-2&oldid=840187620#Test_vectors
	ExpectSha256(sha256("", 0), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	SHA256_CTX Context;

	sha256_init(&Context);
	ExpectSha256(sha256_finish(&Context), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

	// printf 'The quick brown fox jumps over the lazy dog.' | sha256sum
	ExpectSha256(sha256(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c");

	sha256_init(&Context);
	sha256_update(&Context, "The ", 4);
	sha256_update(&Context, "quick ", 6);
	sha256_update(&Context, "brown ", 6);
	sha256_update(&Context, "fox ", 4);
	sha256_update(&Context, "jumps ", 6);
	sha256_update(&Context, "over ", 5);
	sha256_update(&Context, "the ", 4);
	sha256_update(&Context, "lazy ", 5);
	sha256_update(&Context, "dog.", 4);
	ExpectSha256(sha256_finish(&Context), "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c");
}

TEST(Hash, Sha256ToStringSmallBuffer)
{
	ExpectSha256<1>(sha256(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "");
	ExpectSha256<16 + 1>(sha256(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "ef537f25c895bfa7");
}

TEST(Hash, Sha256ToStringLargeBuffer)
{
	ExpectSha256<SHA256_MAXSTRSIZE + 64>(sha256(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c");
}

TEST(Hash, Sha256Eq)
{
	EXPECT_EQ(sha256_comp(sha256("", 0), sha256("", 0)), 0);
	EXPECT_TRUE(sha256("", 0) == sha256("", 0));
	EXPECT_NE(sha256_comp(sha256("a", 1), sha256("b", 1)), 0);
	EXPECT_TRUE(sha256("a", 1) != sha256("b", 1));
}

TEST(Hash, Sha256FromStr)
{
	SHA256_DIGEST Expected = {{
		0x01,
		0x23,
		0x45,
		0x67,
		0x89,
		0x01,
		0x23,
		0x45,
		0x67,
		0x89,
		0x01,
		0x23,
		0x45,
		0x67,
		0x89,
		0x01,
		0x23,
		0x45,
		0x67,
		0x89,
		0x01,
		0x23,
		0x45,
		0x67,
		0x89,
		0x01,
		0x23,
		0x45,
		0x67,
		0x89,
		0x01,
		0x23,
	}};
	SHA256_DIGEST Sha256;
	EXPECT_FALSE(sha256_from_str(&Sha256, "0123456789012345678901234567890123456789012345678901234567890123"));
	EXPECT_EQ(Sha256, Expected);
	EXPECT_TRUE(sha256_from_str(&Sha256, "012345678901234567890123456789012345678901234567890123456789012"));
	EXPECT_TRUE(sha256_from_str(&Sha256, "01234567890123456789012345678901234567890123456789012345678901234"));
	EXPECT_TRUE(sha256_from_str(&Sha256, ""));
	EXPECT_TRUE(sha256_from_str(&Sha256, "012345678901234567890123456789012345678901234567890123456789012x"));
	EXPECT_TRUE(sha256_from_str(&Sha256, "x123456789012345678901234567890123456789012345678901234567890123"));
}

template<size_t BufferSize = MD5_MAXSTRSIZE>
static void ExpectMd5(MD5_DIGEST Actual, const char *pWanted)
{
	char aActual[BufferSize];
	md5_str(Actual, aActual, sizeof(aActual));
	EXPECT_STREQ(aActual, pWanted);
}

TEST(Hash, Md5)
{
	// https://en.wikipedia.org/w/index.php?title=MD5&oldid=889664074#MD5_hashes
	ExpectMd5(md5("", 0), "d41d8cd98f00b204e9800998ecf8427e");
	MD5_CTX Context;

	md5_init(&Context);
	ExpectMd5(md5_finish(&Context), "d41d8cd98f00b204e9800998ecf8427e");

	ExpectMd5(md5(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "e4d909c290d0fb1ca068ffaddf22cbd0");

	md5_init(&Context);
	md5_update(&Context, "The ", 4);
	md5_update(&Context, "quick ", 6);
	md5_update(&Context, "brown ", 6);
	md5_update(&Context, "fox ", 4);
	md5_update(&Context, "jumps ", 6);
	md5_update(&Context, "over ", 5);
	md5_update(&Context, "the ", 4);
	md5_update(&Context, "lazy ", 5);
	md5_update(&Context, "dog.", 4);
	ExpectMd5(md5_finish(&Context), "e4d909c290d0fb1ca068ffaddf22cbd0");
}

TEST(Hash, Md5ToStringSmallBuffer)
{
	ExpectMd5<1>(md5(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "");
	ExpectMd5<16 + 1>(md5(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "e4d909c290d0fb1c");
}

TEST(Hash, Md5ToStringLargeBuffer)
{
	ExpectMd5<MD5_MAXSTRSIZE + 64>(md5(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "e4d909c290d0fb1ca068ffaddf22cbd0");
}

TEST(Hash, Md5Eq)
{
	EXPECT_EQ(md5_comp(md5("", 0), md5("", 0)), 0);
	EXPECT_TRUE(md5("", 0) == md5("", 0));
	EXPECT_NE(md5_comp(md5("a", 1), md5("b", 1)), 0);
	EXPECT_TRUE(md5("a", 1) != md5("b", 1));
}

TEST(Hash, Md5FromStr)
{
	MD5_DIGEST Expected = {{
		0x01,
		0x23,
		0x45,
		0x67,
		0x89,
		0x01,
		0x23,
		0x45,
		0x67,
		0x89,
		0x01,
		0x23,
		0x45,
		0x67,
		0x89,
		0x01,
	}};
	MD5_DIGEST Md5;
	EXPECT_FALSE(md5_from_str(&Md5, "01234567890123456789012345678901"));
	EXPECT_EQ(Md5, Expected);
	EXPECT_TRUE(md5_from_str(&Md5, "0123456789012345678901234567890"));
	EXPECT_TRUE(md5_from_str(&Md5, "012345678901234567890123456789012"));
	EXPECT_TRUE(md5_from_str(&Md5, ""));
	EXPECT_TRUE(md5_from_str(&Md5, "0123456789012345678901234567890x"));
	EXPECT_TRUE(md5_from_str(&Md5, "x1234567890123456789012345678901"));
}
