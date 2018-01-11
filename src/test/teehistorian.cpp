#include <gtest/gtest.h>

#include <base/detect.h>
#include <engine/shared/config.h>
#include <game/gamecore.h>
#include <game/server/teehistorian.h>

void RegisterGameUuids(CUuidManager *pManager);

class TeeHistorian : public ::testing::Test
{
protected:
	CTeeHistorian m_TH;
	CConfiguration m_Config;
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
		#define MACRO_CONFIG_INT(Name,ScriptName,Def,Min,Max,Save,Desc) \
			m_Config.m_##Name = (Def);
		#define MACRO_CONFIG_STR(Name,ScriptName,Len,Def,Save,Desc) \
			str_copy(m_Config.m_##Name, (Def), sizeof(m_Config.m_##Name));
		#include <engine/shared/config_variables.h>
		#undef MACRO_CONFIG_STR
		#undef MACRO_CONFIG_INT

		RegisterUuids(&m_UuidManager);
		RegisterTeehistorianUuids(&m_UuidManager);
		RegisterGameUuids(&m_UuidManager);

		mem_zero(&m_GameInfo, sizeof(m_GameInfo));

		m_GameInfo.m_GameUuid = CalculateUuid("test@ddnet.tw");
		m_GameInfo.m_pServerVersion = "DDNet test";
		m_GameInfo.m_StartTime = time(0);

		m_GameInfo.m_pServerName = "server name";
		m_GameInfo.m_ServerPort = 8303;
		m_GameInfo.m_pGameType = "game type";

		m_GameInfo.m_pMapName = "Kobra 3 Solo";
		m_GameInfo.m_MapSize = 903514;
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
		static const char PREFIX2[] = "\",\"server_name\":\"server name\",\"server_port\":\"8303\",\"game_type\":\"game type\",\"map_name\":\"Kobra 3 Solo\",\"map_size\":\"903514\",\"map_crc\":\"eceaf25c\",\"config\":{},\"tuning\":{},\"uuids\":[";
		static const char PREFIX3[] = "]}";

		char aTimeBuf[64];
		str_timestamp_ex(m_GameInfo.m_StartTime, aTimeBuf, sizeof(aTimeBuf), "%Y-%m-%d %H:%M:%S %z");

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

		if(m_Buffer.Error()
			|| m_Buffer.Size() != OutputSize
			|| mem_comp(m_Buffer.Data(), pOutput, OutputSize) != 0)
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
	Tick(1); Player(0, 1, 2);
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
	Tick(1); Player(0, 1, 2);
	Tick(2); Player(0, 2, 1);
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
	Tick(1); DeadPlayer(0);   Player(1, 2, 3);
	Tick(2); Player(0, 4, 5); Player(1, 2, 3);
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
	Tick(1); Player(0, 4, 5); DeadPlayer(1);
	Tick(2); Player(0, 4, 5); Player(1, 2, 3);
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
	Tick(500); Player(0, -1, -1);
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
	Tick(1); Inputs(); m_TH.RecordPlayerMessage(63, "", 1);
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}

TEST_F(TeeHistorian, ExtraMessage)
{
	const unsigned char EXPECTED[] = {
		0x41, 0x00, // TICK_SKIP dt=0
		// EX uuid=6bb8ba88-0f0b-382e-8dae-dbf4052b8b7d data=""
		0x4a,
		0x6b, 0xb8, 0xba, 0x88, 0x0f, 0x0b, 0x38, 0x2e,
		0x8d, 0xae, 0xdb, 0xf4, 0x05, 0x2b, 0x8b, 0x7d,
		0x00,
		0x40, // FINISH
	};
	Tick(1); Inputs(); m_TH.RecordTestExtra();
	Finish();
	Expect(EXPECTED, sizeof(EXPECTED));
}
