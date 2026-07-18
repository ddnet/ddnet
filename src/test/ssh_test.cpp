#ifdef CONF_SSH

#include <base/str.h>

#include <engine/external/unicode-width/unicode_width.h>
#include <engine/shared/ssh_server.h>

#include <game/client/components/censor.h>

#include <gtest/gtest.h>

TEST(Ssh, History)
{
	CSshClient Client(0, nullptr);
	EXPECT_STREQ(Client.PrevInputFromHistory(), "");
	EXPECT_STREQ(Client.NextInputFromHistory(), "");

	Client.AddToInputHistory("hello");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "hello");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "hello");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "hello");
	EXPECT_STREQ(Client.NextInputFromHistory(), "");

	Client.AddToInputHistory("world");

	EXPECT_STREQ(Client.PrevInputFromHistory(), "world");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "hello");
	EXPECT_STREQ(Client.NextInputFromHistory(), "world");
	EXPECT_STREQ(Client.NextInputFromHistory(), "");

	Client.AddToInputHistory("foo");
	Client.AddToInputHistory("bar");
	EXPECT_STREQ(Client.NextInputFromHistory(), "");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "bar");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "foo");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "world");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "hello");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "hello");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "hello");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "hello");
	EXPECT_STREQ(Client.NextInputFromHistory(), "world");
	EXPECT_STREQ(Client.NextInputFromHistory(), "foo");
	EXPECT_STREQ(Client.NextInputFromHistory(), "bar");
	EXPECT_STREQ(Client.NextInputFromHistory(), "");
	EXPECT_STREQ(Client.NextInputFromHistory(), "");
	EXPECT_STREQ(Client.NextInputFromHistory(), "");
	EXPECT_STREQ(Client.NextInputFromHistory(), "");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "bar");
}

TEST(Ssh, HistoryDuplicates)
{
	CSshClient Client(0, nullptr);
	EXPECT_STREQ(Client.PrevInputFromHistory(), "");
	EXPECT_STREQ(Client.NextInputFromHistory(), "");

	Client.AddToInputHistory("hello");
	Client.AddToInputHistory("world");
	Client.AddToInputHistory("world");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "world");
	EXPECT_STREQ(Client.PrevInputFromHistory(), "hello");
}

TEST(Ssh, LineWrap)
{
	unicode_width_state_t State;
	unicode_width_init(&State);

	char aSshLine[512];
	int Linebreaks;

	Linebreaks = CSshLogger::LineWrapForSsh("hello world", aSshLine, sizeof(aSshLine), 256, &State);
	EXPECT_EQ(Linebreaks, 1);
	EXPECT_STREQ(aSshLine, "hello world");

	Linebreaks = CSshLogger::LineWrapForSsh("hello\nworld", aSshLine, sizeof(aSshLine), 256, &State);
	EXPECT_EQ(Linebreaks, 2);
	EXPECT_STREQ(aSshLine, "hello\r\nworld");

	// this is from a real terminal with width 10
	//
	// +----------+
	// |> test_cmd|
	// |hello worl|
	// |>         |
	// +----------+
	//
	// +----------+
	// |> test_cmd|
	// |hello worl|
	// |d         |
	// |>         |
	// +----------+
	//
	// +----------+
	// |> test_cmd|
	// |hello worl|
	// |d!        |
	// |>         |
	// +----------+

	Linebreaks = CSshLogger::LineWrapForSsh("hello worl", aSshLine, sizeof(aSshLine), 10, &State);
	EXPECT_EQ(Linebreaks, 1);
	EXPECT_STREQ(aSshLine, "hello worl");

	Linebreaks = CSshLogger::LineWrapForSsh("hello world", aSshLine, sizeof(aSshLine), 10, &State);
	EXPECT_EQ(Linebreaks, 2);
	EXPECT_STREQ(aSshLine, "hello world");

	Linebreaks = CSshLogger::LineWrapForSsh("hello world!", aSshLine, sizeof(aSshLine), 10, &State);
	EXPECT_EQ(Linebreaks, 2);
	EXPECT_STREQ(aSshLine, "hello world!");

	Linebreaks = CSshLogger::LineWrapForSsh("✅✅✅✅✅✅✅✅✅✅✅✅✅✅✅", aSshLine, sizeof(aSshLine), 30, &State);
	EXPECT_EQ(Linebreaks, 1);
	EXPECT_STREQ(aSshLine, "✅✅✅✅✅✅✅✅✅✅✅✅✅✅✅");

	Linebreaks = CSshLogger::LineWrapForSsh("✅✅✅✅✅✅✅✅✅✅✅✅✅✅✅", aSshLine, sizeof(aSshLine), 29, &State);
	EXPECT_EQ(Linebreaks, 2);
	EXPECT_STREQ(aSshLine, "✅✅✅✅✅✅✅✅✅✅✅✅✅✅✅");

	Linebreaks = CSshLogger::LineWrapForSsh("abc✅✅✅", aSshLine, sizeof(aSshLine), 36, &State);
	EXPECT_EQ(Linebreaks, 1);
	EXPECT_STREQ(aSshLine, "abc✅✅✅");
}

#endif
