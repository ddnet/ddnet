#include <gtest/gtest.h>

#include <engine/shared/config.h>
#include <game/gamecore.h>
#include <game/server/teehistorian.h>

class TeeHistorian : public ::testing::Test
{
protected:
	CTeeHistorian m_TH;
	CConfiguration m_Config;
	CTuningParams m_Tuning;
	CTeeHistorian::CGameInfo m_GameInfo;

	CPacker m_Buffer;

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
	}

	void Expect(const unsigned char *pOutput, int OutputSize)
	{
		static CUuid TEEHISTORIAN_UUID = CalculateUuid("teehistorian@ddnet.tw");
		static const char PREFIX1[] = "{\"comment\":\"teehistorian@ddnet.tw\",\"version\":\"1\",\"game_uuid\":\"a1eb7182-796e-3b3e-941d-38ca71b2a4a8\",\"server_version\":\"DDNet test\",\"start_time\":\"";
		static const char PREFIX2[] = "\",\"server_name\":\"server name\",\"server_port\":\"8303\",\"game_type\":\"game type\",\"map_name\":\"Kobra 3 Solo\",\"map_size\":\"903514\",\"map_crc\":\"eceaf25c\",\"config\":{},\"tuning\":{}}";

		char aTimeBuf[64];
		str_timestamp_ex(m_GameInfo.m_StartTime, aTimeBuf, sizeof(aTimeBuf), "%Y-%m-%d %H:%M:%S %z");

		CPacker Buffer;
		Buffer.Reset();
		Buffer.AddRaw(&TEEHISTORIAN_UUID, sizeof(TEEHISTORIAN_UUID));
		Buffer.AddRaw(PREFIX1, str_length(PREFIX1));
		Buffer.AddRaw(aTimeBuf, str_length(aTimeBuf));
		Buffer.AddRaw(PREFIX2, str_length(PREFIX2));
		Buffer.AddRaw("", 1);
		Buffer.AddRaw(pOutput, OutputSize);

		ASSERT_FALSE(Buffer.Error());

		ExpectFull(Buffer.Data(), Buffer.Size());
	}

	void ExpectFull(const unsigned char *pOutput, int OutputSize)
	{
		ASSERT_FALSE(m_Buffer.Error());

		const ::testing::TestInfo *pTestInfo =
			::testing::UnitTest::GetInstance()->current_test_info();
		const char *pTestName = pTestInfo->name();

		if(m_Buffer.Size() != OutputSize
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

		ASSERT_EQ(m_Buffer.Size(), OutputSize);
		ASSERT_TRUE(mem_comp(m_Buffer.Data(), pOutput, OutputSize) == 0);
	}
};

TEST_F(TeeHistorian, Empty)
{
	Expect((const unsigned char *)"", 0);
}
