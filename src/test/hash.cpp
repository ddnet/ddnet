#include <gtest/gtest.h>

#include <base/hash_ctxt.h>
#include <base/system.h>

static void Expect(SHA256_DIGEST Actual, const char *pWanted)
{
	char aActual[SHA256_MAXSTRSIZE];
	sha256_str(Actual, aActual, sizeof(aActual));
	EXPECT_STREQ(aActual, pWanted);
}

TEST(Hash, Sha256)
{
	// https://en.wikipedia.org/w/index.php?title=SHA-2&oldid=840187620#Test_vectors
	Expect(sha256("", 0), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	SHA256_CTX ctxt;

	sha256_init(&ctxt);
	Expect(sha256_finish(&ctxt), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

	// printf 'The quick brown fox jumps over the lazy dog.' | sha256sum
	char QUICK_BROWN_FOX[] = "The quick brown fox jumps over the lazy dog.";
	Expect(sha256(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c");

	sha256_init(&ctxt);
	sha256_update(&ctxt, "The ", 4);
	sha256_update(&ctxt, "quick ", 6);
	sha256_update(&ctxt, "brown ", 6);
	sha256_update(&ctxt, "fox ", 4);
	sha256_update(&ctxt, "jumps ", 6);
	sha256_update(&ctxt, "over ", 5);
	sha256_update(&ctxt, "the ", 4);
	sha256_update(&ctxt, "lazy ", 5);
	sha256_update(&ctxt, "dog.", 4);
	Expect(sha256_finish(&ctxt), "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c");
}

TEST(Hash, Sha256Eq)
{
	EXPECT_EQ(sha256("", 0), sha256("", 0));
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

static void Expect2(MD5_DIGEST Actual, const char *pWanted)
{
	char aActual[MD5_MAXSTRSIZE];
	md5_str(Actual, aActual, sizeof(aActual));
	EXPECT_STREQ(aActual, pWanted);
}

TEST(Hash, Md5)
{
	// https://en.wikipedia.org/w/index.php?title=MD5&oldid=889664074#MD5_hashes
	Expect2(md5("", 0), "d41d8cd98f00b204e9800998ecf8427e");
	MD5_CTX ctxt;

	md5_init(&ctxt);
	Expect2(md5_finish(&ctxt), "d41d8cd98f00b204e9800998ecf8427e");

	char QUICK_BROWN_FOX[] = "The quick brown fox jumps over the lazy dog.";
	Expect2(md5(QUICK_BROWN_FOX, str_length(QUICK_BROWN_FOX)), "e4d909c290d0fb1ca068ffaddf22cbd0");

	md5_init(&ctxt);
	md5_update(&ctxt, "The ", 4);
	md5_update(&ctxt, "quick ", 6);
	md5_update(&ctxt, "brown ", 6);
	md5_update(&ctxt, "fox ", 4);
	md5_update(&ctxt, "jumps ", 6);
	md5_update(&ctxt, "over ", 5);
	md5_update(&ctxt, "the ", 4);
	md5_update(&ctxt, "lazy ", 5);
	md5_update(&ctxt, "dog.", 4);
	Expect2(md5_finish(&ctxt), "e4d909c290d0fb1ca068ffaddf22cbd0");
}

TEST(Hash, Md5Eq)
{
	EXPECT_EQ(md5("", 0), md5("", 0));
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
