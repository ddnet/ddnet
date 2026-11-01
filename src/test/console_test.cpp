#include "test.h"

#include <base/color.h>
#include <base/io.h>
#include <base/str.h>
#include <base/types.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>
#include <engine/shared/console.h>

#include <game/version.h>

#include <gtest/gtest.h>

TEST(Console, CreateAndExecuteEcho)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	ASSERT_TRUE(pConsole);

	bool Executed = false;
	class CTestUser
	{
	public:
		bool *m_pExecuted;
	};
	CTestUser User = {&Executed};

	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		(void)pResult;
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		*pUser->m_pExecuted = true;
	};
	pConsole->Register("test_echo", "", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_echo", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_TRUE(Executed);
}

TEST(Console, ArgumentParsingStrings)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		std::string m_Str[4];
		int m_NumArgs;
	};
	CTestUser User;
	User.m_NumArgs = 0;

	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		pUser->m_NumArgs = pResult->NumArguments();
		for(int StringId = 0; StringId < 4; StringId++)
		{
			if(StringId < pUser->m_NumArgs)
				pUser->m_Str[StringId] = std::string(pResult->GetString(StringId));
		}
	};
	pConsole->Register("test_str", "sss", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine(R"(test_str "I'm in space" '' "\"\\Escapes\?\"\n")", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_EQ(User.m_NumArgs, 3);
	EXPECT_STREQ(User.m_Str[0].c_str(), "I'm in space");
	EXPECT_STREQ(User.m_Str[1].c_str(), "''");
	EXPECT_STREQ(User.m_Str[2].c_str(), "\"\\Escapes\\?\"\\n");
	EXPECT_STREQ(User.m_Str[3].c_str(), "");
}

TEST(Console, ArgumentParsingIntegers)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Int[4];
	};
	CTestUser User = {{0, 0, 0, 0}};

	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		for(int IntId = 0; IntId < 4; IntId++)
		{
			pUser->m_Int[IntId] = pResult->GetInteger(IntId);
		}
	};
	pConsole->Register("test_int", "sss", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_int \"1337\" abc -7331def", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_EQ(User.m_Int[0], 1337);
	EXPECT_EQ(User.m_Int[1], 0);
	EXPECT_EQ(User.m_Int[2], 0);
	EXPECT_EQ(User.m_Int[3], 0);
}

TEST(Console, ArgumentParsingFloats)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		float m_Float[4];
	};
	CTestUser User = {{0, 0, 0, 0}};

	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		for(int FloatId = 0; FloatId < 4; FloatId++)
		{
			pUser->m_Float[FloatId] = pResult->GetFloat(FloatId);
		}
	};
	pConsole->Register("test_float", "sss", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_float \"13.37\" abc -73.31def", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_FLOAT_EQ(User.m_Float[0], 13.37f);
	EXPECT_FLOAT_EQ(User.m_Float[1], 0.0f);
	EXPECT_FLOAT_EQ(User.m_Float[2], 0.0f);
	EXPECT_FLOAT_EQ(User.m_Float[3], 0.0f);
}

TEST(Console, ArgumentParsingColors)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		ColorHSLA m_Color[9];
	};
	CTestUser User = {};

	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		for(int ColorId = 0; ColorId < 9; ColorId++)
		{
			pUser->m_Color[ColorId] = pResult->GetColor(ColorId, 0.0f);
		}
	};
	pConsole->Register("test_color", "ssssssss", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_color \"$f00\" $ffa500 $12345 shiny cyan -16777216 $f008 $00ffff80 extra", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_FLOAT_EQ(User.m_Color[0].h, 0.0f);
	EXPECT_FLOAT_EQ(User.m_Color[0].s, 1.0f);
	EXPECT_FLOAT_EQ(User.m_Color[0].l, 0.5f);
	EXPECT_FLOAT_EQ(User.m_Color[0].a, 1.0f);
	EXPECT_FLOAT_EQ(User.m_Color[1].h, 0.10784314f);
	EXPECT_FLOAT_EQ(User.m_Color[4].h, 0.5f);
	EXPECT_FLOAT_EQ(User.m_Color[5].h, 0.0f);
	EXPECT_NEAR(User.m_Color[6].a, 0.53333336f, 0.00001f);
	EXPECT_NEAR(User.m_Color[7].a, 0.5019608f, 0.00001f);
	EXPECT_FLOAT_EQ(User.m_Color[8].h, 0.0f);
}

TEST(Console, ArgumentParsingNumArgumentsOptional)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_NumArgs;
		int m_Count;
	};
	CTestUser User = {0, 0};

	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		if(pUser->m_Count == 0)
			pUser->m_NumArgs = pResult->NumArguments();
		pUser->m_Count++;
	};
	pConsole->Register("test_opt", "s?ss", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_opt one two three", IConsole::CLIENT_ID_UNSPECIFIED, true);
	pConsole->ExecuteLine("test_opt \"one two\" \"three four\"", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_EQ(User.m_Count, 2);
	EXPECT_EQ(User.m_NumArgs, 3);
}

TEST(Console, SemicolonsAndComments)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Count;
	};
	CTestUser User = {0};

	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		(void)pResult;
		static_cast<CTestUser *>(pUserData)->m_Count++;
	};
	pConsole->Register("test_cmd", "", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_cmd; test_cmd # comment", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_EQ(User.m_Count, 2);
}

TEST(Console, InterpretSemicolonsFalse)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Count;
	};
	CTestUser User = {0};

	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		(void)pResult;
		static_cast<CTestUser *>(pUserData)->m_Count++;
	};
	pConsole->Register("test_cmd2", "", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_cmd2", IConsole::CLIENT_ID_UNSPECIFIED, false);
	EXPECT_EQ(User.m_Count, 1);
}

TEST(Console, ChainCommand)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Order[2];
		int m_Index;
	};
	CTestUser User = {{0, 0}, 0};

	auto Original = [](IConsole::IResult *pResult, void *pUserData) {
		(void)pResult;
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		pUser->m_Order[pUser->m_Index++] = 1;
	};
	auto Chained = [](IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		pUser->m_Order[pUser->m_Index++] = 0;
		pfnCallback(pResult, pCallbackUserData);
	};
	pConsole->Register("test_chain", "", CFGFLAG_SERVER, Original, &User, "test");
	pConsole->Chain("test_chain", Chained, &User);
	pConsole->ExecuteLine("test_chain", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_EQ(User.m_Order[0], 0);
	EXPECT_EQ(User.m_Order[1], 1);
}

TEST(Console, RegisterTempAndDeregister)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	pConsole->RegisterTemp("temp_cmd", "", CFGFLAG_SERVER, "temp");
	pConsole->DeregisterTemp("temp_cmd");
	const IConsole::ICommandInfo *pInfo = pConsole->GetCommandInfo("temp_cmd", CFGFLAG_SERVER, true);
	EXPECT_EQ(pInfo, nullptr);
}

TEST(Console, AccessLevelToEnumAndString)
{
	EXPECT_EQ(CConsole::AccessLevelToEnum("admin"), std::optional<IConsole::EAccessLevel>(IConsole::EAccessLevel::ADMIN));
	EXPECT_EQ(CConsole::AccessLevelToEnum("moderator"), std::optional<IConsole::EAccessLevel>(IConsole::EAccessLevel::MODERATOR));
	EXPECT_EQ(CConsole::AccessLevelToEnum("helper"), std::optional<IConsole::EAccessLevel>(IConsole::EAccessLevel::HELPER));
	EXPECT_EQ(CConsole::AccessLevelToEnum("all"), std::optional<IConsole::EAccessLevel>(IConsole::EAccessLevel::USER));
	EXPECT_EQ(CConsole::AccessLevelToEnum("0"), std::optional<IConsole::EAccessLevel>(IConsole::EAccessLevel::ADMIN));
	EXPECT_EQ(CConsole::AccessLevelToEnum("1"), std::optional<IConsole::EAccessLevel>(IConsole::EAccessLevel::MODERATOR));
	EXPECT_EQ(CConsole::AccessLevelToEnum("2"), std::optional<IConsole::EAccessLevel>(IConsole::EAccessLevel::HELPER));
	EXPECT_EQ(CConsole::AccessLevelToEnum("3"), std::optional<IConsole::EAccessLevel>(IConsole::EAccessLevel::USER));

	EXPECT_EQ(CConsole::AccessLevelToEnum("mod"), std::nullopt);
	EXPECT_EQ(CConsole::AccessLevelToEnum("invalid"), std::nullopt);

	EXPECT_STREQ(CConsole::AccessLevelToString(IConsole::EAccessLevel::ADMIN), "admin");
	EXPECT_STREQ(CConsole::AccessLevelToString(IConsole::EAccessLevel::MODERATOR), "moderator");
	EXPECT_STREQ(CConsole::AccessLevelToString(IConsole::EAccessLevel::HELPER), "helper");
	EXPECT_STREQ(CConsole::AccessLevelToString(IConsole::EAccessLevel::USER), "all");
}

TEST(Console, ColorParse)
{
	auto Color = CConsole::ColorParse("$f00", 0.0f);
	ASSERT_TRUE(Color.has_value());
	EXPECT_FLOAT_EQ(Color->h, 0.0f);
	EXPECT_FLOAT_EQ(Color->s, 1.0f);
	EXPECT_FLOAT_EQ(Color->l, 0.5f);

	Color = CConsole::ColorParse("cyan", 0.0f);
	ASSERT_TRUE(Color.has_value());
	EXPECT_FLOAT_EQ(Color->h, 0.5f);

	Color = CConsole::ColorParse("black", 0.5f);
	ASSERT_TRUE(Color.has_value());
	EXPECT_FLOAT_EQ(Color->l, 0.0f);

	Color = CConsole::ColorParse("$00ffff80", 0.0f);
	ASSERT_TRUE(Color.has_value());
	EXPECT_NEAR(Color->a, 0.5019608f, 0.00001f);

	Color = CConsole::ColorParse("invalidcolor", 0.0f);
	EXPECT_FALSE(Color.has_value());
}

TEST(Console, UnknownCommandCallback)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	int UnknownCount = 0;
	auto UnknownCb = [](const char *pCommand, void *pUser) {
		(void)pCommand;
		(*static_cast<int *>(pUser))++;
		return true;
	};
	pConsole->SetUnknownCommandCallback(UnknownCb, &UnknownCount);
	pConsole->ExecuteLine("doesnotexist", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_EQ(UnknownCount, 1);
}

TEST(Console, LineIsValid)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	EXPECT_TRUE(pConsole->LineIsValid("echo hello"));
	EXPECT_FALSE(pConsole->LineIsValid(""));
}

TEST(Console, ExecuteFile)
{
	CTestInfo InfoStorage;
	InfoStorage.m_DeleteTestStorageFilesOnSuccess = true;
	std::unique_ptr<IStorage> pStorage = InfoStorage.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	auto pKernel = std::unique_ptr<IKernel>(IKernel::Create());

	IEngine *pEngine = CreateTestEngine(GAME_NAME);
	pKernel->RegisterInterface(pEngine);

	pKernel->RegisterInterface(pStorage.get(), false);

	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER).release();
	pKernel->RegisterInterface(pConsole);

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/test_execute_file.cfg", InfoStorage.m_aFilename);

	class CTestUser
	{
	public:
		int m_Count;
	};
	CTestUser User = {0};
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		(void)pResult;
		static_cast<CTestUser *>(pUserData)->m_Count++;
	};
	pConsole->Register("file_test_cmd", "", CFGFLAG_SERVER, Callback, &User, "test");

	IOHANDLE File = io_open(aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File != nullptr);
	const char *pContent = "file_test_cmd\nfile_test_cmd\n";
	unsigned int Size = io_write(File, pContent, str_length(pContent));
	ASSERT_TRUE(Size > 0);
	io_close(File);

	// missing storage
	bool Success = pConsole->ExecuteFile(aFilename, IConsole::CLIENT_ID_UNSPECIFIED, true, IStorage::TYPE_ALL);
	EXPECT_FALSE(Success);
	EXPECT_EQ(User.m_Count, 0);

	pEngine->Init();
	pConsole->Init();
	Success = pConsole->ExecuteFile(aFilename, IConsole::CLIENT_ID_UNSPECIFIED, true, IStorage::TYPE_ALL);
	EXPECT_TRUE(Success);

	EXPECT_EQ(User.m_Count, 2);
}

TEST(Console, PossibleCommands)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Count;
	};
	CTestUser User = {0};
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		(void)pResult;
		static_cast<CTestUser *>(pUserData)->m_Count++;
	};
	pConsole->Register("hello_test", "", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->Register("help_test", "", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->PossibleCommands("he", CFGFLAG_SERVER, false, [](int Index, const char *pCmd, void *pUser) {
			(void)Index;
			(void)pCmd;
			static_cast<int *>(pUser)[0]++; }, &User.m_Count);
	EXPECT_GE(User.m_Count, 1);
}

TEST(Console, RemoveArgument)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_NumArgs;
		std::string m_Str[4];
	};
	CTestUser User;
	User.m_NumArgs = 0;
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		if(pResult->NumArguments() >= 3)
			pResult->RemoveArgument(1);
		pUser->m_NumArgs = pResult->NumArguments();
		for(int StringId = 0; StringId < 4; StringId++)
		{
			if(StringId < pUser->m_NumArgs)
				pUser->m_Str[StringId] = std::string(pResult->GetString(StringId));
		}
	};
	pConsole->Register("test_rem", "sss", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_rem a b c", IConsole::CLIENT_ID_UNSPECIFIED, true);
	EXPECT_EQ(User.m_NumArgs, 2);
	EXPECT_STREQ(User.m_Str[0].c_str(), "a");
	EXPECT_STREQ(User.m_Str[1].c_str(), "c");
}

TEST(Console, GetCommandInfo)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	const IConsole::ICommandInfo *pInfo = pConsole->GetCommandInfo("echo", CFGFLAG_SERVER, false);
	EXPECT_TRUE(pInfo != nullptr);
	if(pInfo)
	{
		EXPECT_STREQ(pInfo->Name(), "echo");
	}
}

TEST(Console, VictimSingle)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Count;
		int m_ClientId;
		int m_Victim;
		std::string m_VictimArg;
	};
	CTestUser User = {0, -1, -1, ""};

	auto Resolve = [](int ClientId, const char *pVictim, void *pUserData) -> std::optional<std::vector<int>> {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		pUser->m_ClientId = ClientId;
		if(str_comp(pVictim, "me") == 0)
			return std::vector<int>{7};
		return std::nullopt;
	};
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		pUser->m_Count++;
		pUser->m_Victim = pResult->GetVictim(0);
		pUser->m_VictimArg = pResult->GetString(0);
	};
	pConsole->SetGetVictimsCommandCallback(Resolve, &User);
	pConsole->Register("test_victim", "v", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_victim me", 5, true);
	EXPECT_EQ(User.m_Count, 1);
	EXPECT_EQ(User.m_ClientId, 5);
	EXPECT_EQ(User.m_Victim, 7);
	EXPECT_STREQ(User.m_VictimArg.c_str(), "me");
}

TEST(Console, VictimFanOut)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		std::vector<int> m_aVictims;
	};
	CTestUser User;

	auto Resolve = [](int ClientId, const char *pVictim, void *pUserData) -> std::optional<std::vector<int>> {
		(void)ClientId;
		(void)pUserData;
		if(str_comp(pVictim, "all") == 0)
			return std::vector<int>{1, 2, 3};
		return std::nullopt;
	};
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		pUser->m_aVictims.push_back(pResult->GetVictim(0));
	};
	pConsole->SetGetVictimsCommandCallback(Resolve, &User);
	pConsole->Register("test_victim_all", "v", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_victim_all all", 5, true);
	ASSERT_EQ(User.m_aVictims.size(), 3u);
	EXPECT_EQ(User.m_aVictims[0], 1);
	EXPECT_EQ(User.m_aVictims[1], 2);
	EXPECT_EQ(User.m_aVictims[2], 3);
}

TEST(Console, VictimOptionalDefaultsToMe)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Count;
		int m_Victim;
		std::string m_VictimArg;
	};
	CTestUser User = {0, -1, ""};

	auto Resolve = [](int ClientId, const char *pVictim, void *pUserData) -> std::optional<std::vector<int>> {
		(void)ClientId;
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		pUser->m_VictimArg = pVictim;
		if(str_comp(pVictim, "me") == 0)
			return std::vector<int>{7};
		return std::nullopt;
	};
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		pUser->m_Count++;
		pUser->m_Victim = pResult->GetVictim(0);
	};
	pConsole->SetGetVictimsCommandCallback(Resolve, &User);
	pConsole->Register("test_victim_opt", "?v", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_victim_opt", 5, true);
	EXPECT_EQ(User.m_Count, 1);
	EXPECT_STREQ(User.m_VictimArg.c_str(), "me");
	EXPECT_EQ(User.m_Victim, 7);
}

TEST(Console, VictimInvalid)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Count;
	};
	CTestUser User = {0};

	auto Resolve = [](int ClientId, const char *pVictim, void *pUserData) -> std::optional<std::vector<int>> {
		(void)ClientId;
		(void)pVictim;
		(void)pUserData;
		return std::nullopt;
	};
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		(void)pResult;
		static_cast<CTestUser *>(pUserData)->m_Count++;
	};
	pConsole->SetGetVictimsCommandCallback(Resolve, &User);
	pConsole->Register("test_victim_invalid", "v", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_victim_invalid nobody", 5, true);
	EXPECT_EQ(User.m_Count, 0);
}

TEST(Console, VictimTwoFanOuts)
{
	// Only one parameter may target multiple clients
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Count;
	};
	CTestUser User = {0};

	auto Resolve = [](int ClientId, const char *pVictim, void *pUserData) -> std::optional<std::vector<int>> {
		(void)ClientId;
		(void)pVictim;
		(void)pUserData;
		return std::vector<int>{1, 2};
	};
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		(void)pResult;
		static_cast<CTestUser *>(pUserData)->m_Count++;
	};
	pConsole->SetGetVictimsCommandCallback(Resolve, &User);
	pConsole->Register("test_victim_two", "vv", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_victim_two all all", 5, true);
	EXPECT_EQ(User.m_Count, 0);
}

TEST(Console, VictimEmptyRejected)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		int m_Count;
	};
	CTestUser User = {0};

	auto Resolve = [](int ClientId, const char *pVictim, void *pUserData) -> std::optional<std::vector<int>> {
		(void)ClientId;
		(void)pVictim;
		(void)pUserData;
		return std::vector<int>{7};
	};
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		(void)pResult;
		static_cast<CTestUser *>(pUserData)->m_Count++;
	};
	pConsole->SetGetVictimsCommandCallback(Resolve, &User);
	pConsole->Register("test_victim_empty", "v", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine(R"(test_victim_empty "")", 5, true);
	EXPECT_EQ(User.m_Count, 0);
}

TEST(Console, VictimSinglePlusFanOut)
{
	std::unique_ptr<IConsole> pConsole = CreateConsole(CFGFLAG_SERVER);
	class CTestUser
	{
	public:
		std::vector<std::pair<int, int>> m_aVictims;
	};
	CTestUser User;

	auto Resolve = [](int ClientId, const char *pVictim, void *pUserData) -> std::optional<std::vector<int>> {
		(void)ClientId;
		(void)pUserData;
		if(str_comp(pVictim, "me") == 0)
			return std::vector<int>{7};
		if(str_comp(pVictim, "all") == 0)
			return std::vector<int>{1, 2};
		return std::nullopt;
	};
	auto Callback = [](IConsole::IResult *pResult, void *pUserData) {
		CTestUser *pUser = static_cast<CTestUser *>(pUserData);
		pUser->m_aVictims.emplace_back(pResult->GetVictim(0), pResult->GetVictim(1));
	};
	pConsole->SetGetVictimsCommandCallback(Resolve, &User);
	pConsole->Register("test_victim_mixed", "vv", CFGFLAG_SERVER, Callback, &User, "test");
	pConsole->ExecuteLine("test_victim_mixed me all", 5, true);
	ASSERT_EQ(User.m_aVictims.size(), 2u);
	EXPECT_EQ(User.m_aVictims[0].first, 7);
	EXPECT_EQ(User.m_aVictims[0].second, 1);
	EXPECT_EQ(User.m_aVictims[1].first, 7);
	EXPECT_EQ(User.m_aVictims[1].second, 2);
}
