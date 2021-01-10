#include <gtest/gtest.h>

#include <base/detect.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/gamecore.h>
#include <game/server/teehistorian.h>

void RegisterGameUuids(CUuidManager *pManager);

class TeeHistorian : public ::testing::Test
{
protected:
	CTeeHistorian m_TH;
	CConfig m_Config;
	CTuningParams m_Tuning;
	CUuidManager m_UuidManager;
	CTeeHistorian::CGameInfo m_GameInfo;

	CPacker m_Buffer;

	enum
	{
		STATE_NONE,
		STATE_PLAYERS,
		STATE_INPUTS,
	};

	int m_State;

	TeeHistorian()
	{
		mem_zero(&m_Config, sizeof(m_Config));
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) \
	m_Config.m_##Name = (Def);
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) MACRO_CONFIG_INT(Name, ScriptName, Def, 0, 0, Save, Desc)
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) \
	str_copy(m_Config.m_##Name, (Def), sizeof(m_Config.m_##Name));
#include <engine/shared/config_variables.h>
#undef MACRO_CONFIG_STR
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_INT

		RegisterUuids(&m_UuidManager);
		RegisterTeehistorianUuids(&m_UuidManager);
		RegisterGameUuids(&m_UuidManager);

		SHA256_DIGEST Sha256 = {{0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89,
			0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89,
			0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89,
			0x01, 0x23}};

		mem_zero(&m_GameInfo, sizeof(m_GameInfo));

		m_GameInfo.m_GameUuid = CalculateUuid("test@ddnet.tw");
		m_GameInfo.m_pServerVersion = "DDNet test";
		m_GameInfo.m_StartTime = time(0);
		m_GameInfo.m_pPrngDescription = "test-prng:02468ace";

		m_GameInfo.m_pServerName = "server name";
		m_GameInfo.m_ServerPort = 8303;
		m_GameInfo.m_pGameType = "game type";

		m_GameInfo.m_pMapName = "Kobra 3 Solo";
		m_GameInfo.m_MapSize = 903514;
		m_GameInfo.m_MapSha256 = Sha256;
		m_GameInfo.m_MapCrc = 0xeceaf25c;

		m_GameInfo.m_pConfig = &m_Config;
		m_GameInfo.m_pTuning = &m_Tuning;
		m_GameInfo.m_pUuids = &m_UuidManager;

		Reset(&m_GameInfo);
	}

	static void Write(const void *pData, int DataSize, void *pUser)
	{
		TeeHistorian *pThis = (TeeHistorian *)pUser;
		pThis->m_Buffer.AddRaw(pData, DataSize);
	}

	void Reset(const CTeeHistorian::CGameInfo *pGameInfo)
	{
		m_Buffer.Reset();
		m_TH.Reset(pGameInfo, Write, this);
		m_State = STATE_NONE;
	}

	void Expect(const unsigned char *pOutput, int OutputSize)
	{
		static CUuid TEEHISTORIAN_UUID = CalculateUuid("teehistorian@ddnet.tw");
		static const char PREFIX1[] = "{\"comment\":\"teehistorian@ddnet.tw\",\"version\":\"2\",\"game_uuid\":\"a1eb7182-796e-3b3e-941d-38ca71b2a4a8\",\"server_version\":\"DDNet test\",\"start_time\":\"";
		static const char PREFIX2[] = "\",\"server_name\":\"server name\",\"server_port\":\"8303\",\"game_type\":\"game type\",\"map_name\":\"Kobra 3 Solo\",\"map_size\":\"903514\",\"map_sha256\":\"0123456789012345678901234567890123456789012345678901234567890123\",\"map_crc\":\"eceaf25c\",\"prng_description\":\"test-prng:02468ace\",\"config\":{},\"tuning\":{},\"uuids\":[";
		static const char PREFIX3[] = "]}";

		char aTimeBuf[64];
		str_timestamp_ex(m_GameInfo.m_StartTime, aTimeBuf, sizeof(aTimeBuf), "%Y-%m-%dT%H:%M:%S%z");

		CPacker Buffer;
		Buffer.Reset();
		Buffer.AddRaw(&TEEHISTORIAN_UUID, sizeof(TEEHISTORIAN_UUID));
		Buffer.AddRaw(PREFIX1, str_length(PREFIX1));
		Buffer.AddRaw(aTimeBuf, str_length(aTimeBuf));
		Buffer.AddRaw(PREFIX2, str_length(PREFIX2));
		for(int i = 0; i < m_UuidManager.NumUuids(); i++)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s\"%s\"",
				i == 0 ? "" : ",",
				m_UuidManager.GetName(OFFSET_UUID + i));
			Buffer.AddRaw(aBuf, str_length(aBuf));
		}
		Buffer.AddRaw(PREFIX3, str_length(PREFIX3));
		Buffer.AddRaw("", 1);
		Buffer.AddRaw(pOutput, OutputSize);

		ASSERT_FALSE(Buffer.Error());

		ExpectFull(Buffer.Data(), Buffer.Size());
	}

	void ExpectFull(const unsigned char *pOutput, int OutputSize)
	{
		const ::testing::TestInfo *pTestInfo =
			::testing::UnitTest::GetInstance()->current_test_info();
		const char *pTestName = pTestInfo->name();

		if(m_Buffer.Error() || m_Buffer.Size() != OutputSize || mem_comp(m_Buffer.Data(), pOutput, OutputSize) != 0)
		{
			char aFilename[64];
			IOHANDLE File;

			str_format(aFilename, sizeof(aFilename), "%sGot.teehistorian", pTestName);
			File = io_open(aFilename, IOFLAG_WRITE);
			ASSERT_TRUE(File);
			io_write(File, m_Buffer.Data(), m_Buffer.Size());
			io_close(File);

			str_format(aFilename, sizeof(aFilename), "%sExpected.teehistorian", pTestName);
			File = io_open(aFilename, IOFLAG_WRITE);
			ASSERT_TRUE(File);
			io_write(File, pOutput, OutputSize);
			io_close(File);
		}

		ASSERT_FALSE(m_Buffer.Error());
		ASSERT_EQ(m_Buffer.Size(), OutputSize);
		ASSERT_TRUE(mem_comp(m_Buffer.Data(), pOutput, OutputSize) == 0);
	}

	void Tick(int Tick)
	{
		if(m_State == STATE_PLAYERS)
		{
			Inputs();
		}
		if(m_State == STATE_INPUTS)
		{
			m_TH.EndInputs();
			m_TH.EndTick();
		}
		m_TH.BeginTick(Tick);
		m_TH.BeginPlayers();
		m_State = STATE_PLAYERS;
	}
	void Inputs()
	{
		m_TH.EndPlayers();
		m_TH.BeginInputs();
		m_State = STATE_INPUTS;
	}
	void Finish()
	{
		if(m_State == STATE_PLAYERS)
		{
			Inputs();
		}
		if(m_State == STATE_INPUTS)
		{
			m_TH.EndInputs();
			m_TH.EndTick();
		}
		m_TH.Finish();
	}
	void DeadPlayer(int ClientID)
	{
		m_TH.RecordDeadPlayer(ClientID);
	}
	void Player(int ClientID, int x, int y)
	{
		CNetObj_CharacterCore Char;
		mem_zero(&Char, sizeof(Char));
		Char.m_X = x;
		Char.m_Y = y;
		m_TH.RecordPlayer(ClientID, &Char);
	}
};

TEST_F(TeeHistorian, Empty)
{
	Expect((const unsigned char *)"", 0);
}

TEST_F(TeeHistorian, Finished)
{
	const unsigned char EXPECTED[] = {0x40}; // FINISH
	m_TH.Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, TickImplicitOneTick)
{
	const unsigned char EXPECTED[] = {
		0x42, 0x00, 0x01, 0x02, // PLAYERNEW cid=0 x=1 y=2
		0x40, // FINISH
	};
	Tick(1);
	Player(0, 1, 2);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, TickImplicitTwoTicks)
{
	const unsigned char EXPECTED[] = {
		0x42, 0x00, 0x01, 0x02, // PLAYER_NEW cid=0 x=1 y=2
		0x00, 0x01, 0x40, // PLAYER cid=0 dx=1 dy=-1
		0x40, // FINISH
	};
	Tick(1);
	Player(0, 1, 2);
	Tick(2);
	Player(0, 2, 1);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, TickImplicitDescendingClientID)
{
	const unsigned char EXPECTED[] = {
		0x42, 0x01, 0x02, 0x03, // PLAYER_NEW cid=1 x=2 y=3
		0x42, 0x00, 0x04, 0x05, // PLAYER_NEW cid=0 x=4 y=5
		0x40, // FINISH
	};
	Tick(1);
	DeadPlayer(0);
	Player(1, 2, 3);
	Tick(2);
	Player(0, 4, 5);
	Player(1, 2, 3);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, TickExplicitAscendingClientID)
{
	const unsigned char EXPECTED[] = {
		0x42, 0x00, 0x04, 0x05, // PLAYER_NEW cid=0 x=4 y=5
		0x41, 0x00, // TICK_SKIP dt=0
		0x42, 0x01, 0x02, 0x03, // PLAYER_NEW cid=1 x=2 y=3
		0x40, // FINISH
	};
	Tick(1);
	Player(0, 4, 5);
	DeadPlayer(1);
	Tick(2);
	Player(0, 4, 5);
	Player(1, 2, 3);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, TickImplicitEmpty)
{
	const unsigned char EXPECTED[] = {
		0x40, // FINISH
	};
	for(int i = 1; i < 500; i++)
	{
		Tick(i);
	}
	for(int i = 1000; i < 100000; i += 1000)
	{
		Tick(i);
	}
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, TickExplicitStart)
{
	const unsigned char EXPECTED[] = {
		0x41, 0xb3, 0x07, // TICK_SKIP dt=499
		0x42, 0x00, 0x40, 0x40, // PLAYER_NEW cid=0 x=-1 y=-1
		0x40, // FINISH
	};
	Tick(500);
	Player(0, -1, -1);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, TickExplicitPlayerMessage)
{
	const unsigned char EXPECTED[] = {
		0x41, 0x00, // TICK_SKIP dt=0
		0x46, 0x3f, 0x01, 0x00, // MESSAGE cid=63 msg="\0"
		0x40, // FINISH
	};
	Tick(1);
	Inputs();
	m_TH.RecordPlayerMessage(63, "", 1);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, ExtraMessage)
{
	const unsigned char EXPECTED[] = {
		0x41, 0x00, // TICK_SKIP dt=0
		// EX uuid=6bb8ba88-0f0b-382e-8dae-dbf4052b8b7d data_len=0
		0x4a,
		0x6b, 0xb8, 0xba, 0x88, 0x0f, 0x0b, 0x38, 0x2e,
		0x8d, 0xae, 0xdb, 0xf4, 0x05, 0x2b, 0x8b, 0x7d,
		0x00,
		0x40, // FINISH
	};
	Tick(1);
	Inputs();
	m_TH.RecordTestExtra();
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, DDNetVersion)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=60daba5c-52c4-3aeb-b8ba-b2953fb55a17 data_len=50
		0x4a,
		0x13, 0x97, 0xb6, 0x3e, 0xee, 0x4e, 0x39, 0x19,
		0xb8, 0x6a, 0xb0, 0x58, 0x88, 0x7f, 0xca, 0xf5,
		0x32,
		// (DDNETVER) cid=0 connection_id=fb13a576-d35f-4893-b815-eedc6d98015b
		// ddnet_version=13010 ddnet_version_str=DDNet 13.1 (3623f5e4cd184556)
		0x00,
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b,
		0x92, 0xcb, 0x01, 'D', 'D', 'N', 'e', 't',
		' ', '1', '3', '.', '1', ' ', '(', '3',
		'6', '2', '3', 'f', '5', 'e', '4', 'c',
		'd', '1', '8', '4', '5', '5', '6', ')',
		0x00,
		// EX uuid=41b49541-f26f-325d-8715-9baf4b544ef9 data_len=4
		0x4a,
		0x41, 0xb4, 0x95, 0x41, 0xf2, 0x6f, 0x32, 0x5d,
		0x87, 0x15, 0x9b, 0xaf, 0x4b, 0x54, 0x4e, 0xf9,
		0x04,
		// (DDNETVER_OLD) cid=1 ddnet_version=13010
		0x01, 0x92, 0xcb, 0x01,
		0x40, // FINISH
	};
	CUuid ConnectionID = {
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b};
	m_TH.RecordDDNetVersion(0, ConnectionID, 13010, "DDNet 13.1 (3623f5e4cd184556)");
	m_TH.RecordDDNetVersionOld(1, 13010);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, Auth)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=60daba5c-52c4-3aeb-b8ba-b2953fb55a17 data_len=16
		0x4a,
		0x60, 0xda, 0xba, 0x5c, 0x52, 0xc4, 0x3a, 0xeb,
		0xb8, 0xba, 0xb2, 0x95, 0x3f, 0xb5, 0x5a, 0x17,
		0x10,
		// (AUTH_INIT) cid=0 level=3 auth_name="default_admin"
		0x00, 0x03, 'd', 'e', 'f', 'a', 'u', 'l',
		't', '_', 'a', 'd', 'm', 'i', 'n', 0x00,
		// EX uuid=37ecd3b8-9218-3bb9-a71b-a935b86f6a81 data_len=9
		0x4a,
		0x37, 0xec, 0xd3, 0xb8, 0x92, 0x18, 0x3b, 0xb9,
		0xa7, 0x1b, 0xa9, 0x35, 0xb8, 0x6f, 0x6a, 0x81,
		0x09,
		// (AUTH_LOGIN) cid=1 level=2 auth_name="foobar"
		0x01, 0x02, 'f', 'o', 'o', 'b', 'a', 'r',
		0x00,
		// EX uuid=37ecd3b8-9218-3bb9-a71b-a935b86f6a81 data_len=7
		0x4a,
		0x37, 0xec, 0xd3, 0xb8, 0x92, 0x18, 0x3b, 0xb9,
		0xa7, 0x1b, 0xa9, 0x35, 0xb8, 0x6f, 0x6a, 0x81,
		0x07,
		// (AUTH_LOGIN) cid=1 level=2 auth_name="foobar"
		0x02, 0x01, 'h', 'e', 'l', 'p', 0x00,
		// EX uuid=d4f5abe8-edd2-3fb9-abd8-1c8bb84f4a63 data_len=7
		0x4a,
		0xd4, 0xf5, 0xab, 0xe8, 0xed, 0xd2, 0x3f, 0xb9,
		0xab, 0xd8, 0x1c, 0x8b, 0xb8, 0x4f, 0x4a, 0x63,
		0x01,
		// (AUTH_LOGOUT) cid=1
		0x01,
		0x40, // FINISH
	};
	m_TH.RecordAuthInitial(0, AUTHED_ADMIN, "default_admin");
	m_TH.RecordAuthLogin(1, AUTHED_MOD, "foobar");
	m_TH.RecordAuthLogin(2, AUTHED_HELPER, "help");
	m_TH.RecordAuthLogout(1);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, JoinLeave)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=1899a382-71e3-36da-937d-c9de6bb95b1d data_len=1
		0x4a,
		0x18, 0x99, 0xa3, 0x82, 0x71, 0xe3, 0x36, 0xda,
		0x93, 0x7d, 0xc9, 0xde, 0x6b, 0xb9, 0x5b, 0x1d,
		0x01,
		// (JOINVER6) cid=6
		0x06,
		// JOIN cid=7
		0x47, 0x06,
		// EX uuid=59239b05-0540-318d-bea4-9aa1e80e7d2b data_len=1
		0x4a,
		0x59, 0x23, 0x9b, 0x05, 0x05, 0x40, 0x31, 0x8d,
		0xbe, 0xa4, 0x9a, 0xa1, 0xe8, 0x0e, 0x7d, 0x2b,
		0x01,
		// (JOINVER7) cid=7
		0x07,
		// JOIN cid=7
		0x47, 0x07,
		// LEAVE cid=6 reason="too many pancakes"
		0x48, 0x06, 't', 'o', 'o', ' ', 'm', 'a',
		'n', 'y', ' ', 'p', 'a', 'n', 'c', 'a',
		'k', 'e', 's', 0x00,
		0x40, // FINISH
	};
	m_TH.RecordPlayerJoin(6, CTeeHistorian::PROTOCOL_6);
	m_TH.RecordPlayerJoin(7, CTeeHistorian::PROTOCOL_7);
	m_TH.RecordPlayerDrop(6, "too many pancakes");
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, SaveSuccess)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=4560c756-da29-3036-81d4-90a50f0182cd datalen=42
		0x4a,
		0x45, 0x60, 0xc7, 0x56, 0xda, 0x29, 0x30, 0x36,
		0x81, 0xd4, 0x90, 0xa5, 0x0f, 0x01, 0x82, 0xcd,
		0x1a,
		// team=21
		0x15,
		// save_id
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b,
		// team_save
		'2', '\t', 'H', '.', '\n', 'l', 'l', '0', 0x00,
		// FINISH
		0x40};

	CUuid SaveID = {
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b};
	const char *pTeamSave = "2\tH.\nll0";
	m_TH.RecordTeamSaveSuccess(21, SaveID, pTeamSave);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, SaveFailed)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=b29901d5-1244-3bd0-bbde-23d04b1f7ba9 datalen=42
		0x4a,
		0xb2, 0x99, 0x01, 0xd5, 0x12, 0x44, 0x3b, 0xd0,
		0xbb, 0xde, 0x23, 0xd0, 0x4b, 0x1f, 0x7b, 0xa9,
		0x01,
		// team=12
		0x0c,
		0x40};

	m_TH.RecordTeamSaveFailure(12);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, LoadSuccess)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=e05408d3-a313-33df-9eb3-ddb990ab954a datalen=42
		0x4a,
		0xe0, 0x54, 0x08, 0xd3, 0xa3, 0x13, 0x33, 0xdf,
		0x9e, 0xb3, 0xdd, 0xb9, 0x90, 0xab, 0x95, 0x4a,
		0x1a,
		// team=21
		0x15,
		// save_id
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b,
		// team_save
		'2', '\t', 'H', '.', '\n', 'l', 'l', '0', 0x00,
		// FINISH
		0x40};

	CUuid SaveID = {
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b};
	const char *pTeamSave = "2\tH.\nll0";
	m_TH.RecordTeamLoadSuccess(21, SaveID, pTeamSave);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, LoadFailed)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=ef8905a2-c695-3591-a1cd-53d2015992dd datalen=42
		0x4a,
		0xef, 0x89, 0x05, 0xa2, 0xc6, 0x95, 0x35, 0x91,
		0xa1, 0xcd, 0x53, 0xd2, 0x01, 0x59, 0x92, 0xdd,
		0x01,
		// team=12
		0x0c,
		0x40};

	m_TH.RecordTeamLoadFailure(12);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}
