#include <base/str.h>

#include <engine/shared/ssh_server.h>

#include <game/client/components/censor.h>

#include <gtest/gtest.h>

TEST(Ssh, LineWrap)
{
	char aSshLine[512];
	int Linebreaks;

	Linebreaks = CSshLogger::LineWrapForSsh("hello world", aSshLine, sizeof(aSshLine), 256);
	EXPECT_EQ(Linebreaks, 1);
	EXPECT_STREQ(aSshLine, "hello world");

	Linebreaks = CSshLogger::LineWrapForSsh("hello\nworld", aSshLine, sizeof(aSshLine), 256);
	EXPECT_EQ(Linebreaks, 2);
	EXPECT_STREQ(aSshLine, "hello\r\nworld");

	Linebreaks = CSshLogger::LineWrapForSsh("hello world", aSshLine, sizeof(aSshLine), 10);
	EXPECT_EQ(Linebreaks, 1);
	EXPECT_STREQ(aSshLine, "hello world");

	Linebreaks = CSshLogger::LineWrapForSsh("hello world!", aSshLine, sizeof(aSshLine), 10);
	EXPECT_EQ(Linebreaks, 2);
	EXPECT_STREQ(aSshLine, "hello world!");
}
