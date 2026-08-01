#include <base/fs.h>
#include <base/hash.h>
#include <base/io.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/teehistorian_opcodes.h>
#include <engine/shared/uuid_manager.h>

#include <game/gamecore.h>
#include <game/server/teehistorian.h>
#include <game/server/teehistorian_reader.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace
{

	// Wraps a body byte sequence (as found in `src/test/teehistorian_test.cpp`,
	// which only ever exercises the writer) with a minimal, but complete, header
	// so it can be fed to `CTeeHistorianReader::Open`, embedding a given `version`/
	// `version_minor` instead of the current format's.
	std::vector<unsigned char> BuildFileWithVersion(int Version, int VersionMinor, const std::vector<unsigned char> &vBody)
	{
		static const CUuid s_Uuid = CalculateUuid("teehistorian@ddnet.tw");
		char aJson[512];
		str_format(aJson, sizeof(aJson),
			"{\"comment\":\"teehistorian@ddnet.tw\",\"version\":\"%d\",\"version_minor\":\"%d\","
			"\"game_uuid\":\"a1eb7182-796e-3b3e-941d-38ca71b2a4a8\",\"server_version\":\"DDNet test\","
			"\"start_time\":\"2024-01-01T00:00:00+0000\",\"server_name\":\"server name\","
			"\"server_port\":\"8303\",\"game_type\":\"game type\",\"map_name\":\"Kobra 3 Solo\","
			"\"map_size\":\"903514\",\"map_sha256\":\"0123456789012345678901234567890123456789012345678901234567890123\","
			"\"map_crc\":\"eceaf25c\",\"prng_description\":\"test-prng:02468ace\",\"config\":{},"
			"\"tuning\":{\"gravity\":\"75\"},\"uuids\":[]}",
			Version, VersionMinor);

		std::vector<unsigned char> vResult;
		vResult.resize(sizeof(s_Uuid));
		mem_copy(vResult.data(), &s_Uuid, sizeof(s_Uuid));
		// Includes the JSON's trailing NUL, which is exactly the separator
		// `CTeeHistorianReader::Open` expects.
		vResult.insert(vResult.end(), aJson, aJson + str_length(aJson) + 1);
		vResult.insert(vResult.end(), vBody.begin(), vBody.end());
		return vResult;
	}

	std::vector<unsigned char> BuildFile(const std::vector<unsigned char> &vBody)
	{
		return BuildFileWithVersion(2, 22, vBody);
	}

	// Decodes every event in `vBody` (wrapped via `BuildFile`). Fails the current
	// test via `ASSERT_*` if the header can't be parsed or a genuine decode error
	// occurs; a clean end of buffer without a FINISH chunk is not an error.
	std::vector<CTeeHistorianEvent> DecodeAll(const std::vector<unsigned char> &vFile, CTeeHistorianReader *pReader)
	{
		char aError[256];
		if(!pReader->Open(vFile.data(), vFile.size(), aError, sizeof(aError)))
		{
			ADD_FAILURE() << "Open failed: " << aError;
			return {};
		}
		std::vector<CTeeHistorianEvent> vEvents;
		CTeeHistorianEvent Event;
		while(pReader->NextEvent(&Event))
		{
			vEvents.push_back(Event);
		}
		EXPECT_FALSE(pReader->Error()) << "decode error: " << pReader->ErrorMessage();
		return vEvents;
	}

	std::vector<CTeeHistorianEvent> DecodeBody(const std::vector<unsigned char> &vBody)
	{
		std::vector<unsigned char> vFile = BuildFile(vBody);
		CTeeHistorianReader Reader;
		return DecodeAll(vFile, &Reader);
	}

	// Field-by-field comparison used to verify round-tripped events against the
	// originally decoded ones when a raw byte comparison isn't applicable (see
	// `TeeHistorianReader.RealRecordings`, where CONSOLE_COMMAND chunks are
	// excluded from both sides before calling this).
	// `CheckTick` is disabled by `RealRecordings`: skipping the replay of a
	// CONSOLE_COMMAND chunk that was the sole content of a tick also removes
	// that tick's only evidence from the re-encoded body, which shifts the
	// *inferred* tick number of later purely-implicit (no TICK_SKIP byte)
	// player-data events forward from then on - the same way it would for any
	// consumer fed a copy of the recording with that command's bytes physically
	// cut out. It doesn't affect the type/content of any event, only which
	// absolute tick a later implicit one is attributed to, so it's checked
	// separately (see the size delta logged by the caller) rather than failing
	// the whole comparison.
	::testing::AssertionResult EventsEqual(const CTeeHistorianEvent &A, const CTeeHistorianEvent &B, bool CheckTick = true)
	{
		if(A.m_Type != B.m_Type)
			return ::testing::AssertionFailure() << "type " << A.m_Type << " != " << B.m_Type;
		if(CheckTick && A.m_Tick != B.m_Tick)
			return ::testing::AssertionFailure() << "tick " << A.m_Tick << " != " << B.m_Tick;
		if(A.m_ClientId != B.m_ClientId || A.m_ClientId2 != B.m_ClientId2)
			return ::testing::AssertionFailure() << "client id mismatch";
		if(A.m_X != B.m_X || A.m_Y != B.m_Y)
			return ::testing::AssertionFailure() << "position mismatch";
		if(A.m_Team != B.m_Team || A.m_Practice != B.m_Practice)
			return ::testing::AssertionFailure() << "team/practice mismatch";
		if(mem_comp(&A.m_Input, &B.m_Input, sizeof(CNetObj_PlayerInput)) != 0)
			return ::testing::AssertionFailure() << "input mismatch";
		if(A.m_FlagMask != B.m_FlagMask || A.m_Protocol != B.m_Protocol || A.m_DDNetVersion != B.m_DDNetVersion || A.m_AuthLevel != B.m_AuthLevel || A.m_TimeTicks != B.m_TimeTicks)
			return ::testing::AssertionFailure() << "scalar field mismatch";
		if(A.m_ConnectionId != B.m_ConnectionId || A.m_SaveId != B.m_SaveId || A.m_UnknownUuid != B.m_UnknownUuid)
			return ::testing::AssertionFailure() << "uuid mismatch";
		if(A.m_Str != B.m_Str)
			return ::testing::AssertionFailure() << "string mismatch: '" << A.m_Str << "' != '" << B.m_Str << "'";
		if(A.m_vArgs != B.m_vArgs)
			return ::testing::AssertionFailure() << "args mismatch";
		if(A.m_vRawData != B.m_vRawData)
			return ::testing::AssertionFailure() << "raw data mismatch";
		return ::testing::AssertionSuccess();
	}

	void ExpectInput(const CNetObj_PlayerInput &Actual, int Direction, int TargetX, int TargetY, int Jump, int Fire, int Hook, int PlayerFlags, int WantedWeapon, int NextWeapon, int PrevWeapon)
	{
		EXPECT_EQ(Actual.m_Direction, Direction);
		EXPECT_EQ(Actual.m_TargetX, TargetX);
		EXPECT_EQ(Actual.m_TargetY, TargetY);
		EXPECT_EQ(Actual.m_Jump, Jump);
		EXPECT_EQ(Actual.m_Fire, Fire);
		EXPECT_EQ(Actual.m_Hook, Hook);
		EXPECT_EQ(Actual.m_PlayerFlags, PlayerFlags);
		EXPECT_EQ(Actual.m_WantedWeapon, WantedWeapon);
		EXPECT_EQ(Actual.m_NextWeapon, NextWeapon);
		EXPECT_EQ(Actual.m_PrevWeapon, PrevWeapon);
	}

	// Replays decoded events back through a live `CTeeHistorian`, and returns
	// only the body (i.e. the bytes after the header's NUL terminator), which is
	// what should compare equal to the original body that was decoded. The
	// header itself is not round-tripped (see `RoundTrips` for why).
	class CReplay
	{
	public:
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
		int m_State = STATE_NONE;

		uint32_t m_aUniqueId[MAX_CLIENTS] = {};
		int m_aPendingProtocol[MAX_CLIENTS] = {};

		CReplay()
		{
			mem_zero(&m_Config, sizeof(m_Config));
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) m_Config.m_##Name = (Def);
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) MACRO_CONFIG_INT(Name, ScriptName, Def, 0, 0, Save, Desc)
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) str_copy(m_Config.m_##Name, (Def));
#include <engine/shared/config_variables.h>
#undef MACRO_CONFIG_STR
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_INT

			SHA256_DIGEST Sha256 = {};
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
			m_GameInfo.m_pConfig = &m_Config;
			m_GameInfo.m_pTuning = &m_Tuning;
			m_GameInfo.m_pUuids = &m_UuidManager;

			m_TH.Reset(&m_GameInfo, Write, this);
		}

		static void Write(const void *pData, int DataSize, void *pUser)
		{
			CReplay *pThis = (CReplay *)pUser;
			if(DataSize <= 0)
			{
				return;
			}
			const size_t OldSize = pThis->m_vBuffer.size();
			pThis->m_vBuffer.resize(OldSize + DataSize);
			mem_copy(&pThis->m_vBuffer[OldSize], pData, DataSize);
		}

		void Inputs()
		{
			if(m_State == STATE_PLAYERS)
			{
				m_TH.EndPlayers();
				m_TH.BeginInputs();
				m_State = STATE_INPUTS;
			}
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

		// Returns the body only, i.e. everything after the header's NUL terminator,
		// exactly the way `TeeHistorian::ExpectFull` in teehistorian_test.cpp does.
		std::vector<unsigned char> Body() const
		{
			for(size_t i = 0; i < m_vBuffer.size(); i++)
			{
				if(m_vBuffer[i] == 0)
				{
					return std::vector<unsigned char>(m_vBuffer.begin() + i + 1, m_vBuffer.end());
				}
			}
			return {};
		}
	};

	// Replays `vEvents` through `Replay` via the public `CTeeHistorian` API.
	// `TEEHISTORIAN_READER_EVENT_CONSOLE_COMMAND` is not replayed: doing so
	// faithfully needs a real `IConsole::IResult`, which is orthogonal to
	// whether the reader decoded the chunk correctly (covered separately in
	// `DecodeConsoleCommand`), so replaying it would only exercise IConsole
	// plumbing, not the reader.
	void ReplayEvents(const std::vector<CTeeHistorianEvent> &vEvents, CReplay *pReplay)
	{
		for(const CTeeHistorianEvent &Event : vEvents)
		{
			switch(Event.m_Type)
			{
			case TEEHISTORIAN_READER_EVENT_TICK_SKIP:
				pReplay->Tick(Event.m_Tick);
				break;
			case TEEHISTORIAN_READER_EVENT_PLAYER_NEW:
			case TEEHISTORIAN_READER_EVENT_PLAYER_DIFF:
			{
				CNetObj_CharacterCore Char;
				mem_zero(&Char, sizeof(Char));
				Char.m_X = Event.m_X;
				Char.m_Y = Event.m_Y;
				pReplay->m_TH.RecordPlayer(Event.m_ClientId, &Char);
				break;
			}
			case TEEHISTORIAN_READER_EVENT_PLAYER_OLD:
				pReplay->m_TH.RecordDeadPlayer(Event.m_ClientId);
				break;
			case TEEHISTORIAN_READER_EVENT_INPUT_NEW:
				pReplay->m_aUniqueId[Event.m_ClientId]++;
				pReplay->m_TH.RecordPlayerInput(Event.m_ClientId, pReplay->m_aUniqueId[Event.m_ClientId], &Event.m_Input);
				break;
			case TEEHISTORIAN_READER_EVENT_INPUT_DIFF:
				pReplay->m_TH.RecordPlayerInput(Event.m_ClientId, pReplay->m_aUniqueId[Event.m_ClientId], &Event.m_Input);
				break;
			case TEEHISTORIAN_READER_EVENT_MESSAGE:
				pReplay->m_TH.RecordPlayerMessage(Event.m_ClientId, Event.m_vRawData.data(), Event.m_vRawData.size());
				break;
			case TEEHISTORIAN_READER_EVENT_JOINVER6:
				pReplay->m_aPendingProtocol[Event.m_ClientId] = CTeeHistorian::PROTOCOL_6;
				break;
			case TEEHISTORIAN_READER_EVENT_JOINVER7:
				pReplay->m_aPendingProtocol[Event.m_ClientId] = CTeeHistorian::PROTOCOL_7;
				break;
			case TEEHISTORIAN_READER_EVENT_JOIN:
				pReplay->m_TH.RecordPlayerJoin(Event.m_ClientId, pReplay->m_aPendingProtocol[Event.m_ClientId]);
				break;
			case TEEHISTORIAN_READER_EVENT_DROP:
				pReplay->m_TH.RecordPlayerDrop(Event.m_ClientId, Event.m_Str.c_str());
				break;
			case TEEHISTORIAN_READER_EVENT_CONSOLE_COMMAND:
				break;
			case TEEHISTORIAN_READER_EVENT_FINISH:
				pReplay->Finish();
				break;
			case TEEHISTORIAN_READER_EVENT_PLAYER_TEAM:
				pReplay->m_TH.RecordPlayerTeam(Event.m_ClientId, Event.m_Team);
				break;
			case TEEHISTORIAN_READER_EVENT_TEAM_PRACTICE:
				pReplay->m_TH.RecordTeamPractice(Event.m_Team, Event.m_Practice);
				break;
			case TEEHISTORIAN_READER_EVENT_PLAYER_READY:
				pReplay->m_TH.RecordPlayerReady(Event.m_ClientId);
				break;
			case TEEHISTORIAN_READER_EVENT_PLAYER_REJOIN:
				pReplay->m_TH.RecordPlayerRejoin(Event.m_ClientId);
				break;
			case TEEHISTORIAN_READER_EVENT_ANTIBOT:
				pReplay->m_TH.RecordAntibot(Event.m_vRawData.data(), Event.m_vRawData.size());
				break;
			case TEEHISTORIAN_READER_EVENT_PLAYER_NAME:
				pReplay->m_TH.RecordPlayerName(Event.m_ClientId, Event.m_Str.c_str());
				break;
			case TEEHISTORIAN_READER_EVENT_PLAYER_FINISH:
				pReplay->m_TH.RecordPlayerFinish(Event.m_ClientId, Event.m_TimeTicks);
				break;
			case TEEHISTORIAN_READER_EVENT_TEAM_FINISH:
				pReplay->m_TH.RecordTeamFinish(Event.m_Team, Event.m_TimeTicks);
				break;
			case TEEHISTORIAN_READER_EVENT_PLAYER_SWITCH:
				pReplay->m_TH.RecordPlayerSwap(Event.m_ClientId, Event.m_ClientId2);
				break;
			case TEEHISTORIAN_READER_EVENT_SAVE_SUCCESS:
				pReplay->m_TH.RecordTeamSaveSuccess(Event.m_Team, Event.m_SaveId, Event.m_Str.c_str());
				break;
			case TEEHISTORIAN_READER_EVENT_SAVE_FAILURE:
				pReplay->m_TH.RecordTeamSaveFailure(Event.m_Team);
				break;
			case TEEHISTORIAN_READER_EVENT_LOAD_SUCCESS:
				pReplay->m_TH.RecordTeamLoadSuccess(Event.m_Team, Event.m_SaveId, Event.m_Str.c_str());
				break;
			case TEEHISTORIAN_READER_EVENT_LOAD_FAILURE:
				pReplay->m_TH.RecordTeamLoadFailure(Event.m_Team);
				break;
			case TEEHISTORIAN_READER_EVENT_DDNETVER:
				pReplay->m_TH.RecordDDNetVersion(Event.m_ClientId, Event.m_ConnectionId, Event.m_DDNetVersion, Event.m_Str.c_str());
				break;
			case TEEHISTORIAN_READER_EVENT_DDNETVER_OLD:
				pReplay->m_TH.RecordDDNetVersionOld(Event.m_ClientId, Event.m_DDNetVersion);
				break;
			case TEEHISTORIAN_READER_EVENT_AUTH_INIT:
				pReplay->m_TH.RecordAuthInitial(Event.m_ClientId, Event.m_AuthLevel, Event.m_Str.c_str());
				break;
			case TEEHISTORIAN_READER_EVENT_AUTH_LOGIN:
				pReplay->m_TH.RecordAuthLogin(Event.m_ClientId, Event.m_AuthLevel, Event.m_Str.c_str());
				break;
			case TEEHISTORIAN_READER_EVENT_AUTH_LOGOUT:
				pReplay->m_TH.RecordAuthLogout(Event.m_ClientId);
				break;
			case TEEHISTORIAN_READER_EVENT_TEST:
				pReplay->m_TH.RecordTestExtra();
				break;
			case TEEHISTORIAN_READER_EVENT_UNKNOWN_EX:
				ADD_FAILURE() << "unexpected unknown TEEHISTORIAN_EX chunk in a known fixture";
				break;
			}
		}
	}

	// Decodes `vBody`, replays the result through a fresh `CTeeHistorian`, and
	// asserts the re-encoded body matches the original one byte for byte. This
	// is the round-trip check requested for verification; see the comment on
	// `CReplay::Body` for why only the body (not the header) is compared: the
	// header embeds a formatted `start_time` and non-default config/tuning
	// diffs that can not be losslessly recovered from the parsed JSON strings
	// (`strptime`-ing "%z" back to a `time_t` is platform-fragile, and
	// reproducing arbitrary `CConfig` flag combinations from JSON would require
	// duplicating the writer's full `MACRO_CONFIG_*` walk for values that have
	// nothing to do with what the reader is being verified against). This
	// mirrors `teehistorian_test.cpp`'s own `Expect()/ExpectFull()`, which also
	// never regenerates a header from parsed strings, only from a live
	// `CGameInfo` it already has on hand.
	void AssertBodyRoundTrips(const std::vector<unsigned char> &vBody)
	{
		CTeeHistorianReader Reader;
		std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);

		CReplay Replay;
		ReplayEvents(vEvents, &Replay);

		std::vector<unsigned char> vGot = Replay.Body();
		ASSERT_EQ(vGot, vBody);
	}

	// Appends `Value`, variable-length-int packed the way every field in the format is
	// (`CTeehistorianPacker::AddInt` in teehistorian.cpp), letting corrupt-input tests build a
	// chunk with an arbitrary out-of-range field instead of hand-deriving its packed bytes.
	void AppendVarInt(std::vector<unsigned char> *pBuf, int Value)
	{
		unsigned char aTmp[16];
		unsigned char *pEnd = CVariableInt::Pack(aTmp, Value, sizeof(aTmp));
		pBuf->insert(pBuf->end(), aTmp, pEnd);
	}

	// Builds the body for a single TEEHISTORIAN_EX chunk named `pUuidName`, whose payload is
	// `vFields` varint-packed back to back (every EX chunk's payload is a sequence of `AddInt`s).
	std::vector<unsigned char> BuildExChunkBody(const char *pUuidName, const std::vector<int> &vFields)
	{
		std::vector<unsigned char> vPayload;
		for(int Field : vFields)
		{
			AppendVarInt(&vPayload, Field);
		}

		std::vector<unsigned char> vBody;
		AppendVarInt(&vBody, -TEEHISTORIAN_EX);
		const CUuid Uuid = CalculateUuid(pUuidName);
		const unsigned char *pUuidBytes = (const unsigned char *)&Uuid;
		vBody.insert(vBody.end(), pUuidBytes, pUuidBytes + sizeof(Uuid));
		AppendVarInt(&vBody, (int)vPayload.size());
		vBody.insert(vBody.end(), vPayload.begin(), vPayload.end());
		return vBody;
	}

	// Runs `vBody` to completion and asserts it produced a parse error rather than crashing (a
	// crash would take the whole test process down before any assertion runs) or silently
	// accepting the out-of-range id.
	void ExpectCleanParseError(const std::vector<unsigned char> &vBody)
	{
		std::vector<unsigned char> vFile = BuildFile(vBody);
		CTeeHistorianReader Reader;
		char aError[256];
		ASSERT_TRUE(Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError))) << aError;
		CTeeHistorianEvent Event;
		while(Reader.NextEvent(&Event))
		{
		}
		EXPECT_TRUE(Reader.Error());
	}

} // anonymous namespace

// ---------------------------------------------------------------------
// Section 1: decode byte fixtures taken from `teehistorian_test.cpp` and
// assert the decoded events match what the test constructed there.
// ---------------------------------------------------------------------

TEST(TeeHistorianReader, Empty)
{
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody({});
	EXPECT_TRUE(vEvents.empty());
}

TEST(TeeHistorianReader, Finished)
{
	const std::vector<unsigned char> vBody = {0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_FINISH);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, TickImplicitOneTick)
{
	const std::vector<unsigned char> vBody = {
		0x42, 0x00, 0x01, 0x02, // PLAYER_NEW cid=0 x=1 y=2
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 3u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_TICK_SKIP);
	EXPECT_EQ(vEvents[0].m_Tick, 1);
	EXPECT_FALSE(vEvents[0].m_TickIsExplicit);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_NEW);
	EXPECT_EQ(vEvents[1].m_Tick, 1);
	EXPECT_EQ(vEvents[1].m_ClientId, 0);
	EXPECT_EQ(vEvents[1].m_X, 1);
	EXPECT_EQ(vEvents[1].m_Y, 2);
	EXPECT_EQ(vEvents[2].m_Type, TEEHISTORIAN_READER_EVENT_FINISH);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, TickImplicitTwoTicks)
{
	const std::vector<unsigned char> vBody = {
		0x42, 0x00, 0x01, 0x02, // PLAYER_NEW cid=0 x=1 y=2
		0x00, 0x01, 0x40, // PLAYER cid=0 dx=1 dy=-1
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 5u);
	EXPECT_EQ(vEvents[0].m_Tick, 1);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_NEW);
	EXPECT_EQ(vEvents[1].m_X, 1);
	EXPECT_EQ(vEvents[1].m_Y, 2);
	EXPECT_EQ(vEvents[2].m_Type, TEEHISTORIAN_READER_EVENT_TICK_SKIP);
	EXPECT_EQ(vEvents[2].m_Tick, 2);
	EXPECT_FALSE(vEvents[2].m_TickIsExplicit);
	EXPECT_EQ(vEvents[3].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_DIFF);
	EXPECT_EQ(vEvents[3].m_ClientId, 0);
	EXPECT_EQ(vEvents[3].m_X, 2); // 1 + 1
	EXPECT_EQ(vEvents[3].m_Y, 1); // 2 - 1
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, TickImplicitDescendingClientId)
{
	const std::vector<unsigned char> vBody = {
		0x42, 0x01, 0x02, 0x03, // PLAYER_NEW cid=1 x=2 y=3
		0x42, 0x00, 0x04, 0x05, // PLAYER_NEW cid=0 x=4 y=5
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 5u);
	EXPECT_EQ(vEvents[0].m_Tick, 1);
	EXPECT_EQ(vEvents[1].m_ClientId, 1);
	EXPECT_EQ(vEvents[2].m_Type, TEEHISTORIAN_READER_EVENT_TICK_SKIP);
	EXPECT_EQ(vEvents[2].m_Tick, 2);
	EXPECT_FALSE(vEvents[2].m_TickIsExplicit);
	EXPECT_EQ(vEvents[3].m_ClientId, 0);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, TickExplicitAscendingClientId)
{
	const std::vector<unsigned char> vBody = {
		0x42, 0x00, 0x04, 0x05, // PLAYER_NEW cid=0 x=4 y=5
		0x41, 0x00, // TICK_SKIP dt=0
		0x42, 0x01, 0x02, 0x03, // PLAYER_NEW cid=1 x=2 y=3
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 5u);
	EXPECT_EQ(vEvents[0].m_Tick, 1);
	EXPECT_FALSE(vEvents[0].m_TickIsExplicit);
	EXPECT_EQ(vEvents[2].m_Type, TEEHISTORIAN_READER_EVENT_TICK_SKIP);
	EXPECT_EQ(vEvents[2].m_Tick, 2);
	EXPECT_TRUE(vEvents[2].m_TickIsExplicit);
	EXPECT_EQ(vEvents[2].m_TickDelta, 0);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, TickExplicitStart)
{
	const std::vector<unsigned char> vBody = {
		0x41, 0xb3, 0x07, // TICK_SKIP dt=499
		0x42, 0x00, 0x40, 0x40, // PLAYER_NEW cid=0 x=-1 y=-1
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 3u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_TICK_SKIP);
	EXPECT_TRUE(vEvents[0].m_TickIsExplicit);
	EXPECT_EQ(vEvents[0].m_TickDelta, 499);
	EXPECT_EQ(vEvents[0].m_Tick, 500);
	EXPECT_EQ(vEvents[1].m_X, -1);
	EXPECT_EQ(vEvents[1].m_Y, -1);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, TickExplicitPlayerMessage)
{
	const std::vector<unsigned char> vBody = {
		0x41, 0x00, // TICK_SKIP dt=0
		0x46, 0x3f, 0x01, 0x00, // MESSAGE cid=63 msg="\0"
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 3u);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_MESSAGE);
	EXPECT_EQ(vEvents[1].m_ClientId, 63);
	ASSERT_EQ(vEvents[1].m_vRawData.size(), 1u);
	EXPECT_EQ(vEvents[1].m_vRawData[0], 0);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, ExtraMessage)
{
	const std::vector<unsigned char> vBody = {
		0x41, 0x00, // TICK_SKIP dt=0
		0x4a,
		0x6b, 0xb8, 0xba, 0x88, 0x0f, 0x0b, 0x38, 0x2e,
		0x8d, 0xae, 0xdb, 0xf4, 0x05, 0x2b, 0x8b, 0x7d,
		0x00,
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 3u);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_TEST);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, DDNetVersion)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0x13, 0x97, 0xb6, 0x3e, 0xee, 0x4e, 0x39, 0x19,
		0xb8, 0x6a, 0xb0, 0x58, 0x88, 0x7f, 0xca, 0xf5,
		0x32,
		0x00,
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b,
		0x92, 0xcb, 0x01, 'D', 'D', 'N', 'e', 't',
		' ', '1', '3', '.', '1', ' ', '(', '3',
		'6', '2', '3', 'f', '5', 'e', '4', 'c',
		'd', '1', '8', '4', '5', '5', '6', ')',
		0x00,
		0x4a,
		0x41, 0xb4, 0x95, 0x41, 0xf2, 0x6f, 0x32, 0x5d,
		0x87, 0x15, 0x9b, 0xaf, 0x4b, 0x54, 0x4e, 0xf9,
		0x04,
		0x01, 0x92, 0xcb, 0x01,
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 3u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_DDNETVER);
	EXPECT_EQ(vEvents[0].m_ClientId, 0);
	EXPECT_EQ(vEvents[0].m_DDNetVersion, 13010);
	EXPECT_EQ(vEvents[0].m_Str, "DDNet 13.1 (3623f5e4cd184556)");
	const CUuid ConnectionId = {
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b};
	EXPECT_EQ(vEvents[0].m_ConnectionId, ConnectionId);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_DDNETVER_OLD);
	EXPECT_EQ(vEvents[1].m_ClientId, 1);
	EXPECT_EQ(vEvents[1].m_DDNetVersion, 13010);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, Auth)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0x60, 0xda, 0xba, 0x5c, 0x52, 0xc4, 0x3a, 0xeb,
		0xb8, 0xba, 0xb2, 0x95, 0x3f, 0xb5, 0x5a, 0x17,
		0x10,
		0x00, 0x03, 'd', 'e', 'f', 'a', 'u', 'l',
		't', '_', 'a', 'd', 'm', 'i', 'n', 0x00,
		0x4a,
		0x37, 0xec, 0xd3, 0xb8, 0x92, 0x18, 0x3b, 0xb9,
		0xa7, 0x1b, 0xa9, 0x35, 0xb8, 0x6f, 0x6a, 0x81,
		0x09,
		0x01, 0x02, 'f', 'o', 'o', 'b', 'a', 'r',
		0x00,
		0x4a,
		0x37, 0xec, 0xd3, 0xb8, 0x92, 0x18, 0x3b, 0xb9,
		0xa7, 0x1b, 0xa9, 0x35, 0xb8, 0x6f, 0x6a, 0x81,
		0x07,
		0x02, 0x01, 'h', 'e', 'l', 'p', 0x00,
		0x4a,
		0xd4, 0xf5, 0xab, 0xe8, 0xed, 0xd2, 0x3f, 0xb9,
		0xab, 0xd8, 0x1c, 0x8b, 0xb8, 0x4f, 0x4a, 0x63,
		0x01,
		0x01,
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 5u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_AUTH_INIT);
	EXPECT_EQ(vEvents[0].m_ClientId, 0);
	EXPECT_EQ(vEvents[0].m_AuthLevel, 3);
	EXPECT_EQ(vEvents[0].m_Str, "default_admin");
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_AUTH_LOGIN);
	EXPECT_EQ(vEvents[1].m_ClientId, 1);
	EXPECT_EQ(vEvents[1].m_AuthLevel, 2);
	EXPECT_EQ(vEvents[1].m_Str, "foobar");
	EXPECT_EQ(vEvents[2].m_ClientId, 2);
	EXPECT_EQ(vEvents[2].m_Str, "help");
	EXPECT_EQ(vEvents[3].m_Type, TEEHISTORIAN_READER_EVENT_AUTH_LOGOUT);
	EXPECT_EQ(vEvents[3].m_ClientId, 1);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, JoinLeave)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0x18, 0x99, 0xa3, 0x82, 0x71, 0xe3, 0x36, 0xda,
		0x93, 0x7d, 0xc9, 0xde, 0x6b, 0xb9, 0x5b, 0x1d,
		0x01,
		0x06,
		0x47, 0x06,
		0x4a,
		0x59, 0x23, 0x9b, 0x05, 0x05, 0x40, 0x31, 0x8d,
		0xbe, 0xa4, 0x9a, 0xa1, 0xe8, 0x0e, 0x7d, 0x2b,
		0x01,
		0x07,
		0x47, 0x07,
		0x48, 0x06, 't', 'o', 'o', ' ', 'm', 'a',
		'n', 'y', ' ', 'p', 'a', 'n', 'c', 'a',
		'k', 'e', 's', 0x00,
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 6u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_JOINVER6);
	EXPECT_EQ(vEvents[0].m_ClientId, 6);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_JOIN);
	EXPECT_EQ(vEvents[1].m_ClientId, 6);
	EXPECT_EQ(vEvents[2].m_Type, TEEHISTORIAN_READER_EVENT_JOINVER7);
	EXPECT_EQ(vEvents[2].m_ClientId, 7);
	EXPECT_EQ(vEvents[3].m_Type, TEEHISTORIAN_READER_EVENT_JOIN);
	EXPECT_EQ(vEvents[3].m_ClientId, 7);
	EXPECT_EQ(vEvents[4].m_Type, TEEHISTORIAN_READER_EVENT_DROP);
	EXPECT_EQ(vEvents[4].m_ClientId, 6);
	EXPECT_EQ(vEvents[4].m_Str, "too many pancakes");
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, Input)
{
	const std::vector<unsigned char> vBody = {
		0x41, 0x00,
		0x45,
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
		0x44,
		0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x45,
		0x00, 0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 5u);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_INPUT_NEW);
	EXPECT_EQ(vEvents[1].m_ClientId, 0);
	ExpectInput(vEvents[1].m_Input, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
	EXPECT_EQ(vEvents[2].m_Type, TEEHISTORIAN_READER_EVENT_INPUT_DIFF);
	ExpectInput(vEvents[2].m_Input, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10);
	EXPECT_EQ(vEvents[3].m_Type, TEEHISTORIAN_READER_EVENT_INPUT_NEW);
	ExpectInput(vEvents[3].m_Input, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, SaveSuccess)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0x45, 0x60, 0xc7, 0x56, 0xda, 0x29, 0x30, 0x36,
		0x81, 0xd4, 0x90, 0xa5, 0x0f, 0x01, 0x82, 0xcd,
		0x1a,
		0x15,
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b,
		'2', '\t', 'H', '.', '\n', 'l', 'l', '0', 0x00,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 2u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_SAVE_SUCCESS);
	EXPECT_EQ(vEvents[0].m_Team, 21);
	EXPECT_EQ(vEvents[0].m_Str, "2\tH.\nll0");
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, SaveFailed)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0xb2, 0x99, 0x01, 0xd5, 0x12, 0x44, 0x3b, 0xd0,
		0xbb, 0xde, 0x23, 0xd0, 0x4b, 0x1f, 0x7b, 0xa9,
		0x01,
		0x0c,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 2u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_SAVE_FAILURE);
	EXPECT_EQ(vEvents[0].m_Team, 12);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, LoadSuccess)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0xe0, 0x54, 0x08, 0xd3, 0xa3, 0x13, 0x33, 0xdf,
		0x9e, 0xb3, 0xdd, 0xb9, 0x90, 0xab, 0x95, 0x4a,
		0x1a,
		0x15,
		0xfb, 0x13, 0xa5, 0x76, 0xd3, 0x5f, 0x48, 0x93,
		0xb8, 0x15, 0xee, 0xdc, 0x6d, 0x98, 0x01, 0x5b,
		'2', '\t', 'H', '.', '\n', 'l', 'l', '0', 0x00,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 2u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_LOAD_SUCCESS);
	EXPECT_EQ(vEvents[0].m_Team, 21);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, LoadFailed)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0xef, 0x89, 0x05, 0xa2, 0xc6, 0x95, 0x35, 0x91,
		0xa1, 0xcd, 0x53, 0xd2, 0x01, 0x59, 0x92, 0xdd,
		0x01,
		0x0c,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 2u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_LOAD_FAILURE);
	EXPECT_EQ(vEvents[0].m_Team, 12);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, PlayerSwap)
{
	const std::vector<unsigned char> vBody = {
		0x41, 0x00,
		0x4a,
		0x5d, 0xe9, 0xb6, 0x33, 0x49, 0xcf, 0x3e, 0x99,
		0x9a, 0x25, 0xd4, 0xa7, 0x8e, 0x97, 0x17, 0xd7,
		0x02,
		0x0b,
		0x15,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 3u);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_SWITCH);
	EXPECT_EQ(vEvents[1].m_ClientId, 11);
	EXPECT_EQ(vEvents[1].m_ClientId2, 21);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, PlayerTeam)
{
	const std::vector<unsigned char> vBody = {
		0x41, 0x00,
		0x4a,
		0xa1, 0x11, 0xc0, 0x4e, 0x1e, 0xa8, 0x38, 0xe0,
		0x90, 0xb1, 0xd7, 0xf9, 0x93, 0xca, 0x0d, 0xa9,
		0x02,
		0x21, 0x36,
		0x41, 0x00,
		0x4a,
		0xa1, 0x11, 0xc0, 0x4e, 0x1e, 0xa8, 0x38, 0xe0,
		0x90, 0xb1, 0xd7, 0xf9, 0x93, 0xca, 0x0d, 0xa9,
		0x02,
		0x03, 0x0c,
		0x4a,
		0xa1, 0x11, 0xc0, 0x4e, 0x1e, 0xa8, 0x38, 0xe0,
		0x90, 0xb1, 0xd7, 0xf9, 0x93, 0xca, 0x0d, 0xa9,
		0x02,
		0x21, 0x00,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 6u);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_TEAM);
	EXPECT_EQ(vEvents[1].m_ClientId, 33);
	EXPECT_EQ(vEvents[1].m_Team, 54);
	EXPECT_EQ(vEvents[3].m_ClientId, 3);
	EXPECT_EQ(vEvents[3].m_Team, 12);
	EXPECT_EQ(vEvents[4].m_ClientId, 33);
	EXPECT_EQ(vEvents[4].m_Team, 0);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, TeamPractice)
{
	const std::vector<unsigned char> vBody = {
		0x41, 0x00,
		0x4a,
		0x57, 0x92, 0x83, 0x4e, 0x81, 0xd1, 0x34, 0xc9,
		0xa2, 0x9b, 0xb5, 0xff, 0x25, 0xda, 0xc3, 0xbc,
		0x02,
		0x17, 0x01,
		0x41, 0x00,
		0x4a,
		0x57, 0x92, 0x83, 0x4e, 0x81, 0xd1, 0x34, 0xc9,
		0xa2, 0x9b, 0xb5, 0xff, 0x25, 0xda, 0xc3, 0xbc,
		0x02,
		0x01, 0x01,
		0x4a,
		0x57, 0x92, 0x83, 0x4e, 0x81, 0xd1, 0x34, 0xc9,
		0xa2, 0x9b, 0xb5, 0xff, 0x25, 0xda, 0xc3, 0xbc,
		0x02,
		0x17, 0x00,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 6u);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_TEAM_PRACTICE);
	EXPECT_EQ(vEvents[1].m_Team, 23);
	EXPECT_TRUE(vEvents[1].m_Practice);
	EXPECT_EQ(vEvents[3].m_Team, 1);
	EXPECT_TRUE(vEvents[3].m_Practice);
	EXPECT_EQ(vEvents[4].m_Team, 23);
	EXPECT_FALSE(vEvents[4].m_Practice);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, PlayerRejoinVer6)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0xc1, 0xe9, 0x21, 0xd5, 0x96, 0xf5, 0x37, 0xbb,
		0x8a, 0x45, 0x7a, 0x06, 0xf1, 0x63, 0xd2, 0x7e,
		0x01,
		0x02,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 2u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_REJOIN);
	EXPECT_EQ(vEvents[0].m_ClientId, 2);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, PlayerReadyMultiple)
{
	const std::vector<unsigned char> vBody = {
		0x41, 0x00,
		0x4a,
		0x63, 0x85, 0x87, 0xc9, 0x3f, 0x75, 0x38, 0x87,
		0x91, 0x8e, 0xa3, 0xc2, 0x61, 0x4f, 0xfa, 0xa0,
		0x01,
		0x00,
		0x4a,
		0x63, 0x85, 0x87, 0xc9, 0x3f, 0x75, 0x38, 0x87,
		0x91, 0x8e, 0xa3, 0xc2, 0x61, 0x4f, 0xfa, 0xa0,
		0x01,
		0x0b,
		0x41, 0x00,
		0x4a,
		0x63, 0x85, 0x87, 0xc9, 0x3f, 0x75, 0x38, 0x87,
		0x91, 0x8e, 0xa3, 0xc2, 0x61, 0x4f, 0xfa, 0xa0,
		0x01,
		0x3f,
		0x40};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 6u);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_READY);
	EXPECT_EQ(vEvents[1].m_ClientId, 0);
	EXPECT_EQ(vEvents[2].m_ClientId, 11);
	EXPECT_EQ(vEvents[4].m_ClientId, 63);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, AntibotEmptyNulBytes)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0x86, 0x6b, 0xfd, 0xac, 0xfb, 0x49, 0x3c, 0x0b,
		0xa8, 0x87, 0x5f, 0xe1, 0xf3, 0xea, 0x00, 0xb8,
		0x04,
		0x00, 0x00, 0x00, 0x00};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_ANTIBOT);
	ASSERT_EQ(vEvents[0].m_vRawData.size(), 4u);
	for(unsigned char c : vEvents[0].m_vRawData)
	{
		EXPECT_EQ(c, 0);
	}
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, AntibotEmptyMessage)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0x86, 0x6b, 0xfd, 0xac, 0xfb, 0x49, 0x3c, 0x0b,
		0xa8, 0x87, 0x5f, 0xe1, 0xf3, 0xea, 0x00, 0xb8,
		0x04,
		0xf0, 0x9f, 0xa4, 0x96};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 1u);
	const std::vector<unsigned char> vExpected = {0xf0, 0x9f, 0xa4, 0x96};
	EXPECT_EQ(vEvents[0].m_vRawData, vExpected);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, PlayerName)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0xd0, 0x16, 0xf9, 0xb9, 0x41, 0x51, 0x3b, 0x87,
		0x87, 0xe5, 0x3a, 0x60, 0x87, 0xeb, 0x5f, 0x26,
		0x0e,
		0x15,
		0x6e, 0x61, 0x6d, 0x65, 0x6c, 0x65, 0x73, 0x73,
		0x20, 0x74, 0x65, 0x65, 0x00};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_NAME);
	EXPECT_EQ(vEvents[0].m_ClientId, 21);
	EXPECT_EQ(vEvents[0].m_Str, "nameless tee");
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, PlayerFinish)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0x68, 0x94, 0x3c, 0x01, 0x23, 0x48, 0x3e, 0x01,
		0x94, 0x90, 0x3f, 0x27, 0xf8, 0x26, 0x9d, 0x94,
		0x04,
		0x01, 0x80, 0x89, 0x7a};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_FINISH);
	EXPECT_EQ(vEvents[0].m_ClientId, 1);
	EXPECT_EQ(vEvents[0].m_TimeTicks, 1000000);
	AssertBodyRoundTrips(vBody);
}

TEST(TeeHistorianReader, TeamFinish)
{
	const std::vector<unsigned char> vBody = {
		0x4a,
		0x95, 0x88, 0xb9, 0xaf, 0x3f, 0xdc, 0x37, 0x60,
		0x80, 0x43, 0x82, 0xde, 0xee, 0xe3, 0x17, 0xa5,
		0x03,
		0x3f, 0xa8, 0x0f};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_TEAM_FINISH);
	EXPECT_EQ(vEvents[0].m_Team, 63);
	EXPECT_EQ(vEvents[0].m_TimeTicks, 1000);
	AssertBodyRoundTrips(vBody);
}

// No fixture for TEEHISTORIAN_CONSOLE_COMMAND exists in teehistorian_test.cpp
// (the writer needs a real `IConsole::IResult`, which that file never
// constructs either). The byte layout below is hand-assembled directly from
// `CTeeHistorian::RecordConsoleCommand`'s documented field order, not copied
// from any existing fixture.
TEST(TeeHistorianReader, DecodeConsoleCommand)
{
	const std::vector<unsigned char> vBody = {
		0x49, // -TEEHISTORIAN_CONSOLE_COMMAND
		0x05, // ClientId = 5
		0x03, // FlagMask = 3
		'e', 'c', 'h', 'o', 0x00, // Cmd = "echo"
		0x02, // NumArgs = 2
		'h', 'i', 0x00,
		't', 'h', 'e', 'r', 'e', 0x00,
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 2u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_CONSOLE_COMMAND);
	EXPECT_EQ(vEvents[0].m_ClientId, 5);
	EXPECT_EQ(vEvents[0].m_FlagMask, 3);
	EXPECT_EQ(vEvents[0].m_Str, "echo");
	ASSERT_EQ(vEvents[0].m_vArgs.size(), 2u);
	EXPECT_EQ(vEvents[0].m_vArgs[0], "hi");
	EXPECT_EQ(vEvents[0].m_vArgs[1], "there");
}

// ---------------------------------------------------------------------
// Section 2: header parsing.
// ---------------------------------------------------------------------

TEST(TeeHistorianReader, Header)
{
	std::vector<unsigned char> vFile = BuildFile({0x40});
	CTeeHistorianReader Reader;
	char aError[256];
	ASSERT_TRUE(Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError))) << aError;
	const CTeeHistorianHeader &Header = Reader.Header();
	EXPECT_EQ(Header.m_Version, 2);
	EXPECT_EQ(Header.m_VersionMinor, 22);
	EXPECT_EQ(Header.m_ServerVersion, "DDNet test");
	EXPECT_EQ(Header.m_ServerName, "server name");
	EXPECT_EQ(Header.m_ServerPort, 8303);
	EXPECT_EQ(Header.m_GameType, "game type");
	EXPECT_EQ(Header.m_MapName, "Kobra 3 Solo");
	EXPECT_EQ(Header.m_MapSize, 903514);
	EXPECT_EQ(Header.m_MapCrc, 0xeceaf25c);
	EXPECT_FALSE(Header.m_HavePrevGameUuid);
	// "gravity":"75" in the header (see BuildFile) is a raw fixed-point
	// override (0.75 tuning units), applied on top of the compiled-in default.
	EXPECT_EQ(Header.m_Tuning.m_Gravity.Get(), 75);
	EXPECT_NE(Header.m_Tuning.m_Gravity.Get(), CTuningParams::DEFAULT.m_Gravity.Get());
	EXPECT_EQ(Header.m_Tuning.m_HookLength.Get(), CTuningParams::DEFAULT.m_HookLength.Get());
}

TEST(TeeHistorianReader, RejectsUnsupportedVersion)
{
	std::vector<unsigned char> vFile = BuildFileWithVersion(3, 22, {0x40});
	CTeeHistorianReader Reader;
	char aError[256];
	EXPECT_FALSE(Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError)));
}

TEST(TeeHistorianReader, AcceptsHigherVersionMinor)
{
	// `version_minor` only ever adds new, skippable TEEHISTORIAN_EX chunk types, so a reader
	// older than the file it's reading still understands the whole body; it just can't name
	// whichever new EX chunk motivated the bump. Surfaced via `Header().m_VersionMinor`
	// rather than rejected.
	std::vector<unsigned char> vFile = BuildFileWithVersion(2, 9999, {0x40});
	CTeeHistorianReader Reader;
	char aError[256];
	ASSERT_TRUE(Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError))) << aError;
	EXPECT_EQ(Reader.Header().m_VersionMinor, 9999);
}

// ---------------------------------------------------------------------
// Section 3: malformed/truncated input must fail cleanly, never crash.
// ---------------------------------------------------------------------

TEST(TeeHistorianReader, RejectsBadMagic)
{
	std::vector<unsigned char> vFile = BuildFile({0x40});
	vFile[0] ^= 0xff;
	CTeeHistorianReader Reader;
	char aError[256];
	EXPECT_FALSE(Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError)));
}

TEST(TeeHistorianReader, RejectsTooShort)
{
	const unsigned char aData[4] = {1, 2, 3, 4};
	CTeeHistorianReader Reader;
	char aError[256];
	EXPECT_FALSE(Reader.Open(aData, sizeof(aData), aError, sizeof(aError)));
}

TEST(TeeHistorianReader, RejectsTruncatedHeader)
{
	std::vector<unsigned char> vFile = BuildFile({0x40});
	// Cut off right before the header's NUL terminator.
	vFile.resize(vFile.size() - 2);
	CTeeHistorianReader Reader;
	char aError[256];
	EXPECT_FALSE(Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError)));
}

TEST(TeeHistorianReader, TruncatedBodyIsACleanError)
{
	// PLAYER_NEW with the y coordinate cut off.
	const std::vector<unsigned char> vBody = {0x42, 0x00, 0x01};
	std::vector<unsigned char> vFile = BuildFile(vBody);
	CTeeHistorianReader Reader;
	char aError[256];
	ASSERT_TRUE(Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError)));
	CTeeHistorianEvent Event;
	while(Reader.NextEvent(&Event))
	{
	}
	EXPECT_TRUE(Reader.Error());
	EXPECT_FALSE(Reader.Finished());
}

TEST(TeeHistorianReader, MissingFinishIsNotAnError)
{
	// A recording that just stops (e.g. the server crashed) without ever
	// calling `CTeeHistorian::Finish` is a valid, if incomplete, prefix.
	const std::vector<unsigned char> vBody = {0x42, 0x00, 0x01, 0x02};
	std::vector<unsigned char> vFile = BuildFile(vBody);
	CTeeHistorianReader Reader;
	char aError[256];
	ASSERT_TRUE(Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError)));
	CTeeHistorianEvent Event;
	int NumEvents = 0;
	while(Reader.NextEvent(&Event))
	{
		NumEvents++;
	}
	EXPECT_FALSE(Reader.Error());
	EXPECT_FALSE(Reader.Finished());
	EXPECT_TRUE(Reader.AtEndOfBuffer());
	EXPECT_EQ(NumEvents, 2); // implicit TICK_SKIP + PLAYER_NEW
}

TEST(TeeHistorianReader, RejectsUnknownTopLevelMessageType)
{
	// -20 has no meaning in the current format and, unlike TEEHISTORIAN_EX,
	// carries no length prefix that would let the reader safely skip it.
	const std::vector<unsigned char> vBody = {0x40 | 0x13}; // packs -20
	std::vector<CTeeHistorianEvent> vEvents;
	std::vector<unsigned char> vFile = BuildFile(vBody);
	CTeeHistorianReader Reader;
	char aError[256];
	ASSERT_TRUE(Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError)));
	CTeeHistorianEvent Event;
	while(Reader.NextEvent(&Event))
	{
	}
	EXPECT_TRUE(Reader.Error());
}

TEST(TeeHistorianReader, SkipsUnknownExUuid)
{
	std::vector<unsigned char> vBody = {
		0x4a, // -TEEHISTORIAN_EX
		// A UUID that doesn't match any registered teehistorian-ex chunk.
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
		0x03, // DataSize = 3
		0xaa, 0xbb, 0xcc, // payload, skipped
		0x40, // FINISH
	};
	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	ASSERT_EQ(vEvents.size(), 2u);
	EXPECT_EQ(vEvents[0].m_Type, TEEHISTORIAN_READER_EVENT_UNKNOWN_EX);
	ASSERT_EQ(vEvents[0].m_vRawData.size(), 3u);
	EXPECT_EQ(vEvents[0].m_vRawData[0], 0xaa);
	EXPECT_EQ(vEvents[1].m_Type, TEEHISTORIAN_READER_EVENT_FINISH);

	CTeeHistorianReader Reader2;
	DecodeAll(BuildFile(vBody), &Reader2);
	EXPECT_EQ(Reader2.NumUnknownExChunks(), 1);
	ASSERT_EQ(Reader2.UnknownExUuids().size(), 1u);
	EXPECT_EQ(Reader2.UnknownExUuids()[0].second, 1);
}

// ---------------------------------------------------------------------
// Section 3b: an out-of-range client/team id in an otherwise well-formed chunk must produce a
// clean parse error, never index `m_aPlayers`/`m_aInputs` (or a consumer's own per-client/per-team
// array) out of bounds. Runs every case to completion under a real allocator/sanitizer rather than
// just checking `Reader.Error()`, since a crash would kill the test process, not fail an assertion.
// ---------------------------------------------------------------------

TEST(TeeHistorianReader, CorruptClientIdInPlayerTeamIsACleanError)
{
	// The exact shape of the crash this reproduces: `PLAYER_TEAM` with a client id far outside
	// `[0, MAX_CLIENTS)`, which used to reach `CTeeHistorian::RecordPlayerTeam` unchecked via a
	// harness replaying decoded events (see `ReadClientId`).
	ExpectCleanParseError(BuildExChunkBody("teehistorian-player-team@ddnet.tw", {50000000, 5}));
}

TEST(TeeHistorianReader, CorruptTeamIdInPlayerTeamIsACleanError)
{
	ExpectCleanParseError(BuildExChunkBody("teehistorian-player-team@ddnet.tw", {5, 50000000}));
}

TEST(TeeHistorianReader, CorruptTeamIdInTeamPracticeIsACleanError)
{
	// `CTeeHistorian::RecordTeamPractice` indexes `m_aPrevTeams[MAX_CLIENTS]` (see `ReadTeamId`).
	ExpectCleanParseError(BuildExChunkBody("teehistorian-team-practice@ddnet.tw", {50000000, 1}));
}

TEST(TeeHistorianReader, CorruptClientId2InPlayerSwitchIsACleanError)
{
	ExpectCleanParseError(BuildExChunkBody("teehistorian-player-swap@ddnet.tw", {5, 50000000}));
}

TEST(TeeHistorianReader, CorruptClientIdInJoinIsACleanError)
{
	std::vector<unsigned char> vBody;
	AppendVarInt(&vBody, -TEEHISTORIAN_JOIN);
	AppendVarInt(&vBody, 50000000);
	ExpectCleanParseError(vBody);
}

TEST(TeeHistorianReader, CorruptClientIdInDropIsACleanError)
{
	std::vector<unsigned char> vBody;
	AppendVarInt(&vBody, -TEEHISTORIAN_DROP);
	AppendVarInt(&vBody, 50000000);
	ExpectCleanParseError(vBody);
}

TEST(TeeHistorianReader, CorruptClientIdInMessageIsACleanError)
{
	std::vector<unsigned char> vBody;
	AppendVarInt(&vBody, -TEEHISTORIAN_MESSAGE);
	AppendVarInt(&vBody, 50000000);
	ExpectCleanParseError(vBody);
}

TEST(TeeHistorianReader, CorruptClientIdInConsoleCommandIsACleanError)
{
	std::vector<unsigned char> vBody;
	AppendVarInt(&vBody, -TEEHISTORIAN_CONSOLE_COMMAND);
	AppendVarInt(&vBody, 50000000);
	ExpectCleanParseError(vBody);
}

TEST(TeeHistorianReader, RejectsImplausibleTickSkipDelta)
{
	// The exact shape of the finding: a `TICK_SKIP` delta of 2,147,483,646 (`INT_MAX - 1`) used
	// to run `~2^31` `OnTick()` calls in a consumer replaying the decoded ticks; the reader itself
	// must reject it outright instead of handing a consumer an unusable absolute tick.
	std::vector<unsigned char> vBody;
	AppendVarInt(&vBody, -TEEHISTORIAN_TICK_SKIP);
	AppendVarInt(&vBody, 2147483646);
	ExpectCleanParseError(vBody);
}

TEST(TeeHistorianReader, RejectsTickSkipDeltaOverflow)
{
	// `m_LastWrittenTick + Delta + 1` must not silently wrap even when `Delta` is chosen so the
	// sum would overflow a 32-bit int outright.
	std::vector<unsigned char> vBody;
	AppendVarInt(&vBody, -TEEHISTORIAN_TICK_SKIP);
	AppendVarInt(&vBody, 2147483647);
	ExpectCleanParseError(vBody);
}

TEST(TeeHistorianReader, PositionDeltaOverflowWrapsInsteadOfUb)
{
	// A position this far out is nonsense as an actual coordinate, but decoding it must still be
	// well-defined (two's-complement wraparound, like the input-diff reconstruction already does)
	// rather than signed-overflow undefined behavior.
	std::vector<unsigned char> vBody;
	AppendVarInt(&vBody, -TEEHISTORIAN_PLAYER_NEW);
	AppendVarInt(&vBody, 0); // ClientId
	AppendVarInt(&vBody, 2147483647); // X = INT_MAX
	AppendVarInt(&vBody, 0); // Y
	AppendVarInt(&vBody, 0); // ClientId (bare, non-negative: a position delta)
	AppendVarInt(&vBody, 100); // Dx, pushes X past INT_MAX
	AppendVarInt(&vBody, 0); // Dy

	std::vector<CTeeHistorianEvent> vEvents = DecodeBody(vBody);
	// Each player-data chunk here gets its own implicit TICK_SKIP ahead of it (client id 0 never
	// ascends, see `AdvancePlayerDataTick`): TICK_SKIP, PLAYER_NEW, TICK_SKIP, PLAYER_DIFF.
	ASSERT_EQ(vEvents.size(), 4u);
	EXPECT_EQ(vEvents[3].m_Type, TEEHISTORIAN_READER_EVENT_PLAYER_DIFF);
	EXPECT_EQ(vEvents[3].m_X, -2147483549);
}

// ---------------------------------------------------------------------
// Section 4: parse real recordings, if any are made available via
// DDNET_TEEHISTORIAN_TEST_DIR (a directory of *.teehistorian files). Skipped
// by default so the suite stays hermetic/portable.
// ---------------------------------------------------------------------

TEST(TeeHistorianReader, RealRecordings)
{
	const char *pDir = getenv("DDNET_TEEHISTORIAN_TEST_DIR");
	if(!pDir)
	{
		GTEST_SKIP() << "DDNET_TEEHISTORIAN_TEST_DIR not set";
	}

	std::vector<std::string> vNames;
	fs_listdir(
		pDir, [](const char *pName, int IsDir, int, void *pUser) -> int {
			if(!IsDir && str_endswith(pName, ".teehistorian"))
			{
				((std::vector<std::string> *)pUser)->emplace_back(pName);
			}
			return 0;
		},
		0, &vNames);

	ASSERT_FALSE(vNames.empty()) << "no *.teehistorian files in " << pDir;

	for(const std::string &Name : vNames)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "%s/%s", pDir, Name.c_str());

		IOHANDLE File = io_open(aPath, IOFLAG_READ);
		ASSERT_TRUE(File) << aPath;
		void *pData;
		unsigned DataSize;
		const bool ReadOk = io_read_all(File, &pData, &DataSize);
		io_close(File);
		ASSERT_TRUE(ReadOk) << aPath;
		// Frees `pData` however this iteration ends, including an early `ASSERT_*` return
		// (the sanitizer job runs with `detect_leaks=1`).
		std::unique_ptr<void, decltype(&free)> pDataGuard(pData, free);

		CTeeHistorianReader Reader;
		char aError[256];
		const bool OpenOk = Reader.Open(pData, DataSize, aError, sizeof(aError));
		EXPECT_TRUE(OpenOk) << aPath << ": " << aError;
		if(OpenOk)
		{
			printf("%s: version=%d.%d map=%s map_size=%d\n", aPath,
				Reader.Header().m_Version, Reader.Header().m_VersionMinor,
				Reader.Header().m_MapName.c_str(), Reader.Header().m_MapSize);

			std::vector<CTeeHistorianEvent> vEvents;
			CTeeHistorianEvent Event;
			while(Reader.NextEvent(&Event))
			{
				vEvents.push_back(Event);
			}
			EXPECT_FALSE(Reader.Error()) << aPath << ": " << Reader.ErrorMessage();
			// Not asserted: a recording whose server crashed (or is still being written)
			// legitimately has no FINISH chunk, same as `MissingFinishIsNotAnError` above.
			if(!Reader.Finished())
			{
				printf("%s: no FINISH chunk (recording may be incomplete)\n", aPath);
			}
			EXPECT_EQ(Reader.UnconsumedBytesAfterFinish(), 0u) << aPath;
			printf("%s: %zu events, %d unknown ex chunks\n", aPath, vEvents.size(), Reader.NumUnknownExChunks());

			// Body round-trip: replay the decoded events through a live
			// CTeeHistorian and compare against the original file's body
			// (everything after the header's NUL terminator). See the
			// comment on `AssertBodyRoundTrips` for why only the body, not
			// the header, is compared.
			const unsigned char *pBytes = (const unsigned char *)pData;
			size_t HeaderEnd = 0;
			for(size_t i = 0; i < DataSize; i++)
			{
				if(pBytes[i] == 0)
				{
					HeaderEnd = i + 1;
					break;
				}
			}
			ASSERT_GT(HeaderEnd, 0u) << aPath;
			std::vector<unsigned char> vOriginalBody(pBytes + HeaderEnd, pBytes + DataSize);

			CReplay Replay;
			ReplayEvents(vEvents, &Replay);
			std::vector<unsigned char> vGotBody = Replay.Body();
			if(vGotBody == vOriginalBody)
			{
				printf("%s: body round-trip OK, exact byte match (%zu bytes)\n", aPath, vOriginalBody.size());
			}
			else
			{
				// Real recordings can contain rcon CONSOLE_COMMAND chunks
				// (e.g. admin "record"/"shutdown"), which `ReplayEvents`
				// deliberately doesn't replay (see its comment). A tick
				// whose only content was such a command produces no bytes
				// at all once it's skipped, which can also swallow that
				// tick's TICK_SKIP boundary. Fall back to comparing the
				// substantive (non-TICK_SKIP, non-CONSOLE_COMMAND) decoded
				// events on both sides, in order: this still exercises
				// every other decode/encode path end to end, just without
				// asserting exact tick numbers immediately around a
				// filtered-out command.
				std::vector<CTeeHistorianEvent> vExpected;
				std::vector<bool> vExpectedPastConsoleCommand;
				bool PastConsoleCommand = false;
				for(const CTeeHistorianEvent &Ev : vEvents)
				{
					if(Ev.m_Type == TEEHISTORIAN_READER_EVENT_CONSOLE_COMMAND)
					{
						PastConsoleCommand = true;
						continue;
					}
					if(Ev.m_Type != TEEHISTORIAN_READER_EVENT_TICK_SKIP)
					{
						vExpected.push_back(Ev);
						vExpectedPastConsoleCommand.push_back(PastConsoleCommand);
					}
				}
				std::vector<CTeeHistorianEvent> vGotEventsRaw = DecodeBody(vGotBody);
				std::vector<CTeeHistorianEvent> vGotEvents;
				for(const CTeeHistorianEvent &Ev : vGotEventsRaw)
				{
					if(Ev.m_Type != TEEHISTORIAN_READER_EVENT_TICK_SKIP)
					{
						vGotEvents.push_back(Ev);
					}
				}
				ASSERT_EQ(vExpected.size(), vGotEvents.size()) << aPath;
				for(size_t i = 0; i < vExpected.size(); i++)
				{
					EXPECT_TRUE(EventsEqual(vExpected[i], vGotEvents[i], /*CheckTick=*/!vExpectedPastConsoleCommand[i])) << aPath << " event " << i;
				}
				int NumConsoleCommands = 0;
				for(const CTeeHistorianEvent &Ev : vEvents)
				{
					NumConsoleCommands += Ev.m_Type == TEEHISTORIAN_READER_EVENT_CONSOLE_COMMAND;
				}
				printf("%s: body round-trip matches modulo %d CONSOLE_COMMAND chunk(s) not replayed (%zu vs %zu bytes)\n",
					aPath, NumConsoleCommands, vGotBody.size(), vOriginalBody.size());
			}
		}
	}
}
