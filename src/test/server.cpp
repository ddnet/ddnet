#include <engine/server/server.h>

#include <gtest/gtest.h>

TEST(Server, StrHideIps)
{
	char aLine[512];
	char aLineWithoutIps[512];
	bool HasHiddenIps = false;

	HasHiddenIps = CServer::StrHideIps("hello world", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_FALSE(HasHiddenIps);
	EXPECT_STREQ(aLine, "hello world");
	EXPECT_STREQ(aLineWithoutIps, "hello world");

	HasHiddenIps = CServer::StrHideIps("hello <{127.0.0.1}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "hello 127.0.0.1");
	EXPECT_STREQ(aLineWithoutIps, "hello XXX");

	HasHiddenIps = CServer::StrHideIps("<{127.0.0.1}> <{127.0.0.1}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "127.0.0.1 127.0.0.1");
	EXPECT_STREQ(aLineWithoutIps, "XXX XXX");

	HasHiddenIps = CServer::StrHideIps("o <{127.0.0.1}>", aLine, 8, aLineWithoutIps, 8);
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "o 127.0");
	EXPECT_STREQ(aLineWithoutIps, "o XXX");

	HasHiddenIps = CServer::StrHideIps("o <{127.0.0.1}>", aLine, 4, aLineWithoutIps, 4);
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "o 1");
	EXPECT_STREQ(aLineWithoutIps, "o X");

	HasHiddenIps = CServer::StrHideIps("<{}><{}> hello", aLine, 10, aLineWithoutIps, 10);
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, " hell");
	EXPECT_STREQ(aLineWithoutIps, "XXXXXX h");

	HasHiddenIps = CServer::StrHideIps("<{", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_FALSE(HasHiddenIps);
	EXPECT_STREQ(aLine, "<{");
	EXPECT_STREQ(aLineWithoutIps, "<{");

	HasHiddenIps = CServer::StrHideIps("<{}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "");
	EXPECT_STREQ(aLineWithoutIps, "XXX");

	HasHiddenIps = CServer::StrHideIps("<{<{<{<{<{<{<{", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_FALSE(HasHiddenIps);
	EXPECT_STREQ(aLine, "<{<{<{<{<{<{<{");
	EXPECT_STREQ(aLineWithoutIps, "<{<{<{<{<{<{<{");

	HasHiddenIps = CServer::StrHideIps("<{<{<{<{<{<{<{a}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "<{<{<{<{<{<{a");
	EXPECT_STREQ(aLineWithoutIps, "XXX");

	HasHiddenIps = CServer::StrHideIps("<{}><{}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "");
	EXPECT_STREQ(aLineWithoutIps, "XXXXXX");

	HasHiddenIps = CServer::StrHideIps("<{}>}>}>}>}>}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "}>}>}>}>}>");
	EXPECT_STREQ(aLineWithoutIps, "XXX}>}>}>}>}>");

	HasHiddenIps = CServer::StrHideIps("<{<{<{a}>}>}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "a");
	EXPECT_STREQ(aLineWithoutIps, "XXX}>}>");

	HasHiddenIps = CServer::StrHideIps("<{127.0.0.1}> <{127.0.0.1}> <{127.0.0.1}>", aLine, sizeof(aLine), aLineWithoutIps, sizeof(aLineWithoutIps));
	EXPECT_TRUE(HasHiddenIps);
	EXPECT_STREQ(aLine, "127.0.0.1 127.0.0.1 127.0.0.1");
	EXPECT_STREQ(aLineWithoutIps, "XXX XXX XXX");
}
