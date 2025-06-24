#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

#if defined(CONF_FAMILY_WINDOWS)

TEST(ArgumentsToWide, SingleArgumentNoQuotes)
{
	const char *apArguments[] = {"change_map ctf5"};
	std::wstring result = windows_args_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, LR"("change_map ctf5")");
}

TEST(ArgumentsToWide, SingleArgumentWithQuotes)
{
	const char *apArguments[] = {"change_map \"ctf5\""};
	std::wstring result = windows_args_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, L"\"change_map \\\"ctf5\\\"\"");
}

TEST(ArgumentsToWide, MultipleArguments)
{
	const char *apArguments[] = {"change_map ctf5", "sv_register 0"};
	std::wstring result = windows_args_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, LR"("change_map ctf5" "sv_register 0")");
}

TEST(ArgumentsToWide, SingleArgumentWithSlashes)
{
	const char *apArguments[] = {R"(sv_name te\st)"};
	std::wstring result = windows_args_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, L"\"sv_name te\\st\"");
}

TEST(ArgumentsToWide, MultipleArgumentsWithSlashesAndQuotes)
{
	const char *apArguments[] = {R"(sv_name "te\\st")", R"(sv_motd "fo\\o")"};
	std::wstring result = windows_args_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, L"\"sv_name \\\"te\\\\st\\\"\" \"sv_motd \\\"fo\\\\o\\\"\"");
}

TEST(ArgumentsToWide, MultipleArgumentsWithEndSlash)
{
	const char *apArguments[] = {R"(sv_name "te\\st\\")", R"(sv_motd foo\)"};
	std::wstring result = windows_args_to_wide(apArguments, std::size(apArguments));
	EXPECT_EQ(result, L"\"sv_name \\\"te\\\\st\\\\\\\\\\\"\" \"sv_motd foo\\\\\"");
}

#endif
