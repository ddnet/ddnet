#include <gtest/gtest.h>

#include <base/detect.h>
#include <engine/external/json-parser/json.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/gamecore.h>
#include <game/server/teehistorian.h>

#include <vector>

void RegisterGameUuids(CUuidManager *pManager);

class TeeHistorian : public ::testing::Test
{
protected:
	CTeeHistorian m_TH;
	CConfig m_Config;
	CTuningParams m_Tuning;
	CUuidManager m_UuidManager;
	CTeeHistorian::CGameInfo m_GameInfo;

	std::vector<unsigned char> m_vBuffer;

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
		m_GameInfo.m_StartTime = time(nullptr);
		m_GameInfo.m_pPrngDescription = "test-prng:02468ace";

		m_GameInfo.m_pServerName = "server name";
		m_GameInfo.m_ServerPort = 8303;
		m_GameInfo.m_pGameType = "game type";

		m_GameInfo.m_pMapName = "Kobra 3 Solo";
		m_GameInfo.m_MapSize = 903514;
		m_GameInfo.m_MapSha256 = Sha256;
		m_GameInfo.m_MapCrc = 0xeceaf25c;

		m_GameInfo.m_HavePrevGameUuid = false;
		mem_zero(&m_GameInfo.m_PrevGameUuid, sizeof(m_GameInfo.m_PrevGameUuid));

		m_GameInfo.m_pConfig = &m_Config;
		m_GameInfo.m_pTuning = &m_Tuning;
		m_GameInfo.m_pUuids = &m_UuidManager;

		Reset(&m_GameInfo);
	}

	static void WriteBuffer(std::vector<unsigned char> &vBuffer, const void *pData, size_t DataSize)
	{
		if(DataSize <= 0)
			return;

		const size_t OldSize = vBuffer.size();
		vBuffer.resize(OldSize + DataSize);
		mem_copy(&vBuffer[OldSize], pData, DataSize);
	}

	static void Write(const void *pData, int DataSize, void *pUser)
	{
		TeeHistorian *pThis = (TeeHistorian *)pUser;
		WriteBuffer(pThis->m_vBuffer, pData, DataSize);
	}

	void Reset(const CTeeHistorian::CGameInfo *pGameInfo)
	{
		m_vBuffer.clear();
		m_TH.Reset(pGameInfo, Write, this);
		m_State = STATE_NONE;
	}

	void Expect(const unsigned char *pOutput, size_t OutputSize)
	{
		static CUuid TEEHISTORIAN_UUID = CalculateUuid("teehistorian@ddnet.tw");
		static const char PREFIX1[] = "{\"comment\":\"teehistorian@ddnet.tw\",\"version\":\"2\",\"version_minor\":\"9\",\"game_uuid\":\"a1eb7182-796e-3b3e-941d-38ca71b2a4a8\",\"server_version\":\"DDNet test\",\"start_time\":\"";
		static const char PREFIX2[] = "\",\"server_name\":\"server name\",\"server_port\":\"8303\",\"game_type\":\"game type\",\"map_name\":\"Kobra 3 Solo\",\"map_size\":\"903514\",\"map_sha256\":\"0123456789012345678901234567890123456789012345678901234567890123\",\"map_crc\":\"eceaf25c\",\"prng_description\":\"test-prng:02468ace\",\"config\":{},\"tuning\":{},\"uuids\":[";
		static const char PREFIX3[] = "]}";

		char aTimeBuf[64];
		str_timestamp_ex(m_GameInfo.m_StartTime, aTimeBuf, sizeof(aTimeBuf), "%Y-%m-%dT%H:%M:%S%z");

		std::vector<unsigned char> vBuffer;
		WriteBuffer(vBuffer, &TEEHISTORIAN_UUID, sizeof(TEEHISTORIAN_UUID));
		WriteBuffer(vBuffer, PREFIX1, str_length(PREFIX1));
		WriteBuffer(vBuffer, aTimeBuf, str_length(aTimeBuf));
		WriteBuffer(vBuffer, PREFIX2, str_length(PREFIX2));
		for(int i = 0; i < m_UuidManager.NumUuids(); i++)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s\"%s\"",
				i == 0 ? "" : ",",
				m_UuidManager.GetName(OFFSET_UUID + i));
			WriteBuffer(vBuffer, aBuf, str_length(aBuf));
		}
		WriteBuffer(vBuffer, PREFIX3, str_length(PREFIX3));
		WriteBuffer(vBuffer, "", 1);
		WriteBuffer(vBuffer, pOutput, OutputSize);

		ExpectFull(vBuffer.data(), vBuffer.size());
	}

	void ExpectFull(const unsigned char *pOutput, size_t OutputSize)
	{
		const ::testing::TestInfo *pTestInfo =
			::testing::UnitTest::GetInstance()->current_test_info();
		const char *pTestName = pTestInfo->name();

		if(m_vBuffer.size() != OutputSize || mem_comp(m_vBuffer.data(), pOutput, OutputSize) != 0)
		{
			char aFilename[IO_MAX_PATH_LENGTH];
			IOHANDLE File;

			str_format(aFilename, sizeof(aFilename), "%sGot.teehistorian", pTestName);
			File = io_open(aFilename, IOFLAG_WRITE);
			ASSERT_TRUE(File);
			io_write(File, m_vBuffer.data(), m_vBuffer.size());
			io_close(File);

			str_format(aFilename, sizeof(aFilename), "%sExpected.teehistorian", pTestName);
			File = io_open(aFilename, IOFLAG_WRITE);
			ASSERT_TRUE(File);
			io_write(File, pOutput, OutputSize);
			io_close(File);
		}

		printf("pOutput = {");
		size_t Start = 0; // skip over header;
		for(size_t i = 0; i < m_vBuffer.size(); i++)
		{
			if(Start == 0)
			{
				if(m_vBuffer[i] == 0)
					Start = i + 1;
				continue;
			}
			if((i - Start) % 10 == 0)
				printf("\n\t");
			else
				printf(", ");
			printf("0x%.2x", m_vBuffer[i]);
		}
		printf("\n}\n");
		ASSERT_EQ(m_vBuffer.size(), OutputSize);
		ASSERT_TRUE(mem_comp(m_vBuffer.data(), pOutput, OutputSize) == 0);
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
	void DeadPlayer(int ClientId)
	{
		m_TH.RecordDeadPlayer(ClientId);
	}
	void Player(int ClientId, int x, int y)
	{
		CNetObj_CharacterCore Char;
		mem_zero(&Char, sizeof(Char));
		Char.m_X = x;
		Char.m_Y = y;
		m_TH.RecordPlayer(ClientId, &Char);
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

TEST_F(TeeHistorian, TickImplicitDescendingClientId)
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

TEST_F(TeeHistorian, TickExplicitAscendingClientId)
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
	CUuid ConnectionId = {
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b};
	m_TH.RecordDDNetVersion(0, ConnectionId, 13010, "DDNet 13.1 (3623f5e4cd184556)");
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

TEST_F(TeeHistorian, Input)
{
	CNetObj_PlayerInput Input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	const unsigned char EXPECTED[] = {
		// TICK_SKIP dt=0
		0x41, 0x00,
		// new player -> InputNew
		0x45,
		0x00, // ClientId 0
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
		// same unique id, same input -> nothing
		// same unique id, different input -> InputDiff
		0x44,
		0x00, // ClientId 0
		0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		// different unique id, same input -> InputNew
		0x45,
		0x00, // ClientId 0
		0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
		// FINISH
		0x40};

	Tick(1);

	// new player -> InputNew
	m_TH.RecordPlayerInput(0, 1, &Input);
	// same unique id, same input -> nothing
	m_TH.RecordPlayerInput(0, 1, &Input);

	Input.m_Direction = 0;

	// same unique id, different input -> InputDiff
	m_TH.RecordPlayerInput(0, 1, &Input);

	// different unique id, same input -> InputNew
	m_TH.RecordPlayerInput(0, 2, &Input);

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

	CUuid SaveId = {
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b};
	const char *pTeamSave = "2\tH.\nll0";
	m_TH.RecordTeamSaveSuccess(21, SaveId, pTeamSave);
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

	CUuid SaveId = {
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b};
	const char *pTeamSave = "2\tH.\nll0";
	m_TH.RecordTeamLoadSuccess(21, SaveId, pTeamSave);
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

TEST_F(TeeHistorian, PlayerSwap)
{
	const unsigned char EXPECTED[] = {
		// TICK_SKIP dt=0
		0x41, 0x00,
		// EX uuid=5de9b633-49cf-3e99-9a25-d4a78e9717d7 datalen=2
		0x4a,
		0x5d, 0xe9, 0xb6, 0x33, 0x49, 0xcf, 0x3e, 0x99,
		0x9a, 0x25, 0xd4, 0xa7, 0x8e, 0x97, 0x17, 0xd7,
		0x02,
		// playerId1=11
		0x0b,
		// playerId2=21
		0x15,
		// FINISH
		0x40};
	Tick(1);
	m_TH.RecordPlayerSwap(11, 21);
	Finish();

	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, PlayerTeam)
{
	const unsigned char EXPECTED[] = {
		// TICK_SKIP dt=0
		0x41, 0x00,
		// EX uuid=a111c04e-1ea8-38e0-90b1-d7f993ca0da9 datalen=2
		0x4a,
		0xa1, 0x11, 0xc0, 0x4e, 0x1e, 0xa8, 0x38, 0xe0,
		0x90, 0xb1, 0xd7, 0xf9, 0x93, 0xca, 0x0d, 0xa9,
		0x02,
		// (PLAYER_TEAM) cid=33 team=54
		0x21, 0x36,
		// TICK_SKIP dt=0
		0x41, 0x00,
		// EX uuid=a111c04e-1ea8-38e0-90b1-d7f993ca0da9 datalen=2
		0x4a,
		0xa1, 0x11, 0xc0, 0x4e, 0x1e, 0xa8, 0x38, 0xe0,
		0x90, 0xb1, 0xd7, 0xf9, 0x93, 0xca, 0x0d, 0xa9,
		0x02,
		// (PLAYER_TEAM) cid=3 team=12
		0x03, 0x0c,
		// EX uuid=a111c04e-1ea8-38e0-90b1-d7f993ca0da9 datalen=2
		0x4a,
		0xa1, 0x11, 0xc0, 0x4e, 0x1e, 0xa8, 0x38, 0xe0,
		0x90, 0xb1, 0xd7, 0xf9, 0x93, 0xca, 0x0d, 0xa9,
		0x02,
		// (PLAYER_TEAM) cid=33 team=0
		0x21, 0x00,
		// FINISH
		0x40};

	Tick(1);
	m_TH.RecordPlayerTeam(3, 0);
	m_TH.RecordPlayerTeam(33, 54);
	m_TH.RecordPlayerTeam(45, 0);
	Tick(2);
	m_TH.RecordPlayerTeam(3, 12);
	m_TH.RecordPlayerTeam(33, 0);
	m_TH.RecordPlayerTeam(45, 0);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, TeamPractice)
{
	const unsigned char EXPECTED[] = {
		// TICK_SKIP dt=0
		0x41, 0x00,
		// EX uuid=5792834e-81d1-34c9-a29b-b5ff25dac3bc datalen=2
		0x4a,
		0x57, 0x92, 0x83, 0x4e, 0x81, 0xd1, 0x34, 0xc9,
		0xa2, 0x9b, 0xb5, 0xff, 0x25, 0xda, 0xc3, 0xbc,
		0x02,
		// (TEAM_PRACTICE) team=23 practice=1
		0x17, 0x01,
		// TICK_SKIP dt=0
		0x41, 0x00,
		// EX uuid=5792834e-81d1-34c9-a29b-b5ff25dac3bc datalen=2
		0x4a,
		0x57, 0x92, 0x83, 0x4e, 0x81, 0xd1, 0x34, 0xc9,
		0xa2, 0x9b, 0xb5, 0xff, 0x25, 0xda, 0xc3, 0xbc,
		0x02,
		// (TEAM_PRACTICE) team=1 practice=1
		0x01, 0x01,
		// EX uuid=5792834e-81d1-34c9-a29b-b5ff25dac3bc datalen=2
		0x4a,
		0x57, 0x92, 0x83, 0x4e, 0x81, 0xd1, 0x34, 0xc9,
		0xa2, 0x9b, 0xb5, 0xff, 0x25, 0xda, 0xc3, 0xbc,
		0x02,
		// (TEAM_PRACTICE) team=23 practice=0
		0x17, 0x00,
		// FINISH
		0x40};

	Tick(1);
	m_TH.RecordTeamPractice(1, false);
	m_TH.RecordTeamPractice(16, false);
	m_TH.RecordTeamPractice(23, true);
	Tick(2);
	m_TH.RecordTeamPractice(1, true);
	m_TH.RecordTeamPractice(16, false);
	m_TH.RecordTeamPractice(23, false);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, PlayerRejoinVer6)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=c1e921d5-96f5-37bb-8a45-7a06f163d27e datalen=1
		0x4a,
		0xc1, 0xe9, 0x21, 0xd5, 0x96, 0xf5, 0x37, 0xbb,
		0x8a, 0x45, 0x7a, 0x06, 0xf1, 0x63, 0xd2, 0x7e,
		0x01,
		// (PLAYER_REJOIN) cid=2
		0x02,
		// FINISH
		0x40};

	m_TH.RecordPlayerRejoin(2);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, PlayerReady)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=638587c9-3f75-3887-918e-a3c2614ffaa0 datalen=1
		0x4a,
		0x63, 0x85, 0x87, 0xc9, 0x3f, 0x75, 0x38, 0x87,
		0x91, 0x8e, 0xa3, 0xc2, 0x61, 0x4f, 0xfa, 0xa0,
		0x01,
		// (PLAYER_READY) cid=63
		0x3f,
		// FINISH
		0x40};

	m_TH.RecordPlayerReady(63);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, PlayerReadyMultiple)
{
	const unsigned char EXPECTED[] = {
		// TICK_SKIP dt=0
		0x41, 0x00,
		// EX uuid=638587c9-3f75-3887-918e-a3c2614ffaa0 datalen=1
		0x4a,
		0x63, 0x85, 0x87, 0xc9, 0x3f, 0x75, 0x38, 0x87,
		0x91, 0x8e, 0xa3, 0xc2, 0x61, 0x4f, 0xfa, 0xa0,
		0x01,
		// (PLAYER_READY) cid=0
		0x00,
		// EX uuid=638587c9-3f75-3887-918e-a3c2614ffaa0 datalen=1
		0x4a,
		0x63, 0x85, 0x87, 0xc9, 0x3f, 0x75, 0x38, 0x87,
		0x91, 0x8e, 0xa3, 0xc2, 0x61, 0x4f, 0xfa, 0xa0,
		0x01,
		// (PLAYER_READY) cid=11
		0x0b,
		// TICK_SKIP dt=0
		0x41, 0x00,
		// EX uuid=638587c9-3f75-3887-918e-a3c2614ffaa0 datalen=1
		0x4a,
		0x63, 0x85, 0x87, 0xc9, 0x3f, 0x75, 0x38, 0x87,
		0x91, 0x8e, 0xa3, 0xc2, 0x61, 0x4f, 0xfa, 0xa0,
		0x01,
		// (PLAYER_READY) cid=63
		0x3f,
		0x40};

	Tick(1);
	m_TH.RecordPlayerReady(0);
	m_TH.RecordPlayerReady(11);
	Tick(2);
	m_TH.RecordPlayerReady(63);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, AntibotEmpty)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=866bfdac-fb49-3c0b-a887-5fe1f3ea00b8 datalen=0
		0x4a,
		0x86, 0x6b, 0xfd, 0xac, 0xfb, 0x49, 0x3c, 0x0b,
		0xa8, 0x87, 0x5f, 0xe1, 0xf3, 0xea, 0x00, 0xb8,
		0x00,
		// (ANTIBOT) antibot_data
	};

	m_TH.RecordAntibot("", 0);
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, AntibotEmptyNulBytes)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=866bfdac-fb49-3c0b-a887-5fe1f3ea00b8 datalen=4
		0x4a,
		0x86, 0x6b, 0xfd, 0xac, 0xfb, 0x49, 0x3c, 0x0b,
		0xa8, 0x87, 0x5f, 0xe1, 0xf3, 0xea, 0x00, 0xb8,
		0x04,
		// (ANTIBOT) antibot_data
		0x00,
		0x00,
		0x00,
		0x00};

	m_TH.RecordAntibot("\0\0\0\0", 4);
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, AntibotEmptyMessage)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=866bfdac-fb49-3c0b-a887-5fe1f3ea00b8 datalen=4
		0x4a,
		0x86, 0x6b, 0xfd, 0xac, 0xfb, 0x49, 0x3c, 0x0b,
		0xa8, 0x87, 0x5f, 0xe1, 0xf3, 0xea, 0x00, 0xb8,
		0x04,
		// (ANTIBOT) antibot_data
		0xf0,
		0x9f,
		0xa4,
		0x96};

	m_TH.RecordAntibot("🤖", 4);
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, PlayerName)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=d016f9b9-4151-3b87-87e5-3a6087eb5f26 datalen=14
		0x4a,
		0xd0, 0x16, 0xf9, 0xb9, 0x41, 0x51, 0x3b, 0x87,
		0x87, 0xe5, 0x3a, 0x60, 0x87, 0xeb, 0x5f, 0x26,
		0x0e,
		// (PLAYER_NAME) id=21 name="nameless tee"
		0x15,
		0x6e, 0x61, 0x6d, 0x65, 0x6c, 0x65, 0x73, 0x73,
		0x20, 0x74, 0x65, 0x65, 0x00};

	m_TH.RecordPlayerName(21, "nameless tee");
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, PlayerFinish)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=68943c01-2348-3e01-9490-3f27f8269d94 datalen=4
		0x4a,
		0x68, 0x94, 0x3c, 0x01, 0x23, 0x48, 0x3e, 0x01,
		0x94, 0x90, 0x3f, 0x27, 0xf8, 0x26, 0x9d, 0x94,
		0x04,
		// (PLAYER_FINISH) id=1 time=1000000
		0x01, 0x80, 0x89, 0x7a};

	m_TH.RecordPlayerFinish(1, 1000000);
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, TeamFinish)
{
	const unsigned char EXPECTED[] = {
		// EX uuid=9588b9af-3fdc-3760-8043-82deeee317a5 datalen=3
		0x4a,
		0x95, 0x88, 0xb9, 0xaf, 0x3f, 0xdc, 0x37, 0x60,
		0x80, 0x43, 0x82, 0xde, 0xee, 0xe3, 0x17, 0xa5,
		0x03,
		// (TEAM_FINISH) team=63 Time=1000
		0x3f, 0xa8, 0x0f};

	m_TH.RecordTeamFinish(63, 1000);
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, PrevGameUuid)
{
	m_GameInfo.m_HavePrevGameUuid = true;
	CUuid PrevGameUuid = {{
		// fe19c218-f555-4002-a273-126c59ccc17a
		0xfe, 0x19, 0xc2, 0x18, 0xf5, 0x55, 0x40, 0x02,
		0xa2, 0x73, 0x12, 0x6c, 0x59, 0xcc, 0xc1, 0x7a,
		//
	}};
	m_GameInfo.m_PrevGameUuid = PrevGameUuid;
	Reset(&m_GameInfo);
	Finish();
	json_value *pJson = json_parse((const char *)m_vBuffer.data() + 16, -1);
	ASSERT_TRUE(pJson);
	const json_value &JsonPrevGameUuid = (*pJson)["prev_game_uuid"];
	ASSERT_EQ(JsonPrevGameUuid.type, json_string);
	EXPECT_STREQ(JsonPrevGameUuid, "fe19c218-f555-4002-a273-126c59ccc17a");
	json_value_free(pJson);
}
