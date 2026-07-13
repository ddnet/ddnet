#ifdef CONF_SSH

#include <base/str.h>

#include <engine/external/unicode-width/unicode_width.h>
#include <engine/shared/ssh_server.h>

#include <game/client/components/censor.h>

#include <gtest/gtest.h>

TEST(Ssh, LineWrap)
{
	unicode_width_state_t State;
	unicode_width_init(&State);

	char aSshLine[512];
	int Linebreaks;

	// Linebreaks = CSshLogger::LineWrapForSsh("hello world", aSshLine, sizeof(aSshLine), 256);
	// EXPECT_EQ(Linebreaks, 1);
	// EXPECT_STREQ(aSshLine, "hello world");

	// Linebreaks = CSshLogger::LineWrapForSsh("hello\nworld", aSshLine, sizeof(aSshLine), 256);
	// EXPECT_EQ(Linebreaks, 2);
	// EXPECT_STREQ(aSshLine, "hello\r\nworld");

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

	// Linebreaks = CSshLogger::LineWrapForSsh("hello worl", aSshLine, sizeof(aSshLine), 10);
	// EXPECT_EQ(Linebreaks, 1);
	// EXPECT_STREQ(aSshLine, "hello worl");

	// Linebreaks = CSshLogger::LineWrapForSsh("hello world", aSshLine, sizeof(aSshLine), 10);
	// EXPECT_EQ(Linebreaks, 2);
	// EXPECT_STREQ(aSshLine, "hello world");

	// Linebreaks = CSshLogger::LineWrapForSsh("hello world!", aSshLine, sizeof(aSshLine), 10);
	// EXPECT_EQ(Linebreaks, 2);
	// EXPECT_STREQ(aSshLine, "hello world!");

	// Linebreaks = CSshLogger::LineWrapForSsh("✅✅✅✅✅✅✅✅✅✅✅✅✅✅✅", aSshLine, sizeof(aSshLine), 36);
	// EXPECT_EQ(Linebreaks, 1);
	// EXPECT_STREQ(aSshLine, "✅✅✅✅✅✅✅✅✅✅✅✅✅✅✅");

	Linebreaks = CSshLogger::LineWrapForSsh("abc✅✅✅", aSshLine, sizeof(aSshLine), 36, &State);
	EXPECT_EQ(Linebreaks, 1);
	EXPECT_STREQ(aSshLine, "abc✅✅✅");
}

#endif
