#include <gtest/gtest.h>

#include <base/system.h>

TEST(OpenLink, InvalidFile)
{
	EXPECT_EQ(open_link("file:///invalid/path/to/file.txt"), 1);
}

TEST(OpenLink, SimpleShellInjection)
{
	EXPECT_EQ(open_link("exit 1"), 1);
	EXPECT_EQ(open_link(";exit 1"), 1);
	EXPECT_EQ(open_link(";$(exit 1)"), 1);
	EXPECT_EQ(open_link("`exit 1`"), 1);
	EXPECT_EQ(open_link("'\\''$(exit 1)'\\''"), 1);
	EXPECT_EQ(open_link(";exit 1'\\"), 1);
}

TEST(OpenLink, TodoSinglequoteEscape)
{
	EXPECT_EQ(open_link("--help' >/dev/null 2>&1; exit 1; '"), 0);
}
