#include <engine/server/server.h>

#include <gtest/gtest.h>

TEST(Server, StrHideIps)
{
	char aLine[512];
	char aLineWithoutIps[512];

	CServer::StrHideIps("hello world", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "hello world");
	EXPECT_STREQ(aLineWithoutIps, "hello world");

	CServer::StrHideIps("hello <{127.0.0.1}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "hello 127.0.0.1");
	EXPECT_STREQ(aLineWithoutIps, "hello XXX");

	CServer::StrHideIps("<{127.0.0.1}> <{127.0.0.1}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "127.0.0.1 <{127.0.0.1}>");
	EXPECT_STREQ(aLineWithoutIps, "XXX <{127.0.0.1}>");

	CServer::StrHideIps("o <{127.0.0.1}>", aLine, 8, aLineWithoutIps, 8);
	EXPECT_STREQ(aLine, "o 127.0");
	EXPECT_STREQ(aLineWithoutIps, "o XXX");

	CServer::StrHideIps("o <{127.0.0.1}>", aLine, 4, aLineWithoutIps, 4);
	EXPECT_STREQ(aLine, "o 1");
	EXPECT_STREQ(aLineWithoutIps, "o X");

	CServer::StrHideIps("<{}><{}> hello", aLine, 10, aLineWithoutIps, 10);
	EXPECT_STREQ(aLine, "<{}> hell");
	EXPECT_STREQ(aLineWithoutIps, "XXX<{}> h");

	CServer::StrHideIps("<{", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "<{");
	EXPECT_STREQ(aLineWithoutIps, "<{");

	CServer::StrHideIps("<{}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "");
	EXPECT_STREQ(aLineWithoutIps, "XXX");

	CServer::StrHideIps("<{<{<{<{<{<{<{", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "<{<{<{<{<{<{<{");
	EXPECT_STREQ(aLineWithoutIps, "<{<{<{<{<{<{<{");

	CServer::StrHideIps("<{<{<{<{<{<{<{a}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "<{<{<{<{<{<{a");
	EXPECT_STREQ(aLineWithoutIps, "XXX");

	CServer::StrHideIps("<{}><{}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "<{}>");
	EXPECT_STREQ(aLineWithoutIps, "XXX<{}>");

	CServer::StrHideIps("<{}>}>}>}>}>}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "}>}>}>}>}>");
	EXPECT_STREQ(aLineWithoutIps, "XXX}>}>}>}>}>");

	CServer::StrHideIps("<{<{<{a}>}>}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_STREQ(aLine, "<{<{a}>}>");
	EXPECT_STREQ(aLineWithoutIps, "XXX}>}>");
}
