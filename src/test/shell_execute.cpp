#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

TEST(ArgumentsToWide, SingleArgumentNoQuotes)
{
	const char *apArguments[] = {"change_map ctf5"};
	std::wstring result = arguments_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, LR"("change_map ctf5")");
}

TEST(ArgumentsToWide, SingleArgumentWithQuotes)
{
	const char *apArguments[] = {"change_map \"ctf5\""};
	std::wstring result = arguments_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, LR"("change_map """ctf5"""")");
}

TEST(ArgumentsToWide, MultipleArguments)
{
	const char *apArguments[] = {"change_map ctf5", "sv_register 0"};
	std::wstring result = arguments_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, LR"("change_map ctf5" "sv_register 0")");
}

TEST(ArgumentsToWide, SingleArgumentWithSlashes)
{
	const char *apArguments[] = {"sv_name \"te\\st\""};
	std::wstring result = arguments_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, LR"("sv_name """te\st"""")");
}

TEST(ArgumentsToWide, MultipleArgumentWithSlashesAndQuotes)
{
	const char *apArguments[] = {"sv_name \"te\\st\"", "change_map \"ctf5\""};
	std::wstring result = arguments_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, LR"("sv_name """te\st"""" "change_map """ctf5"""")");
}
