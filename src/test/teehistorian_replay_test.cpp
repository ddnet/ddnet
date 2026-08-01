#include "gameworld_test.h"

#include <base/fs.h>
#include <base/hash.h>
#include <base/io.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/uuid_manager.h>
#include <engine/storage.h>

#include <game/prng.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teehistorian.h>
#include <game/server/teehistorian_reader.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------
// Feeds a recorded teehistorian session's inputs back into a real
// `CGameWorld` (on the same map the recording was made on) and compares the
// resulting character positions against the positions the recording says
// actually happened. See `TeeHistorianReplay.RealRecordings` below.
// ---------------------------------------------------------------------

namespace
{

	// A single (tick, client) position mismatch found during replay.
	struct CDivergence
	{
		int m_Tick;
		int m_ClientId;
		int m_RecordedX, m_RecordedY;
		int m_ReplayedX, m_ReplayedY;
	};

	struct CReplayReport
	{
		// Set when the recording's header doesn't match the map the fixture loaded; nothing else in
		// this report is meaningful in that case.
		std::string m_MapMismatchReason;

		// False when the recording had no usable prng_description, so random draws (teleport
		// destinations) cannot be reproduced.
		bool m_PrngSeeded = false;

		bool m_ReaderError = false;
		std::string m_ReaderErrorMessage;

		int m_TicksReplayed = 0;
		int m_PositionsCompared = 0;
		int m_DivergentComparisons = 0;
		// Distinct ticks with at least one divergent client, i.e. `<= m_DivergentComparisons`.
		int m_DivergentTicks = 0;
		// The first 10 divergent (tick, client) comparisons, in the order they were found.
		std::vector<CDivergence> m_vFirstDivergences;
		// PLAYER_DIFF events seen for a client the replay never saw a PLAYER_NEW for, meaning the
		// recording started mid-session (persistent state across a map reload); the replay force-spawns
		// such a client at the diff's position instead of the actual (unrecorded) spawn point.
		int m_ImplicitSpawns = 0;
		int m_aDivergentPerClient[MAX_CLIENTS] = {0};
		int m_aFirstDivergentTick[MAX_CLIENTS] = {0};
		// CONSOLE_COMMAND, MESSAGE and PLAYER_TEAM events seen but not replayed (see `ApplyEvent`):
		// console/chat commands (/rescue, /kill, /pause, /tp, /swap, ...) and client messages
		// (Cl_Kill, Cl_SetTeam) can move or reposition a character, and team assignment changes
		// collision/hook/solo. Nonzero makes this recording's comparison inconclusive, not a clean
		// physics pass: any divergence (or lack of one) could be explained by an event this harness
		// never applied rather than by the physics it's actually meant to check.
		int m_NotReplayedEvents = 0;
		// Aborted because a gap between two consecutive events exceeded `MAX_TICK_GAP`; nothing in
		// this report past the abort point is meaningful.
		bool m_AbortedTickGapTooLarge = false;
		int m_AbortedAtTick = 0;
	};

	// Per-client bookkeeping the replay needs to mirror what the recording implies about a player,
	// without inventing information the recording doesn't actually contain (e.g. spawn selection).
	struct CReplayClient
	{
		bool m_Joined = false;
		bool m_Alive = false;
		vec2 m_RecordedPos = vec2(0.0f, 0.0f);
		CNetObj_PlayerInput m_PendingInput = {};
	};

	// Upper bound, in ticks, on a single gap this harness will simulate between two consecutive
	// teehistorian events (see the loop in `Replay`). `CTeeHistorianReader` already rejects an
	// absolute tick past a multi-decade bound (`MAX_PLAUSIBLE_TICK`), which stops a corrupt
	// `TICK_SKIP` delta from overflowing, but a merely large one (e.g. a few hundred thousand
	// ticks) would still pass that check and then cost this harness one real `OnTick()` call per
	// simulated tick. 200 seconds of continuous silence (10,000 ticks at the server's fixed 50
	// ticks/second) is already far more than any short `coverage` test recording plausibly has,
	// so a gap past it means the recording is corrupt or hostile, not idle; abort that recording
	// (report it, don't silently truncate) rather than let the test run unboundedly long.
	constexpr int MAX_TICK_GAP = 10'000;

	// Builds a real, byte-for-byte valid teehistorian file (header and body) via a live
	// `CTeeHistorian`, so a corrupt-tick-skip test can hand `CReplayWorld::Replay` something it
	// would otherwise only ever see from disk. Mirrors `teehistorian_reader_test.cpp`'s `CReplay`.
	class CMinimalRecordingWriter
	{
	public:
		CTeeHistorian m_TH;
		CConfig m_Config;
		CTuningParams m_Tuning;
		CUuidManager m_UuidManager;
		CTeeHistorian::CGameInfo m_GameInfo;
		std::vector<unsigned char> m_vBuffer;

		CMinimalRecordingWriter()
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
			// Matches the map `GameWorld`/`CReplayWorld` load, so `Replay` doesn't skip this file
			// as a map mismatch.
			m_GameInfo.m_pMapName = "coverage";
			m_GameInfo.m_MapSize = 3805;
			m_GameInfo.m_MapSha256 = Sha256;
			m_GameInfo.m_MapCrc = 0;
			m_GameInfo.m_HavePrevGameUuid = false;
			m_GameInfo.m_pConfig = &m_Config;
			m_GameInfo.m_pTuning = &m_Tuning;
			m_GameInfo.m_pUuids = &m_UuidManager;

			m_TH.Reset(&m_GameInfo, Write, this);
		}

		static void Write(const void *pData, int DataSize, void *pUser)
		{
			CMinimalRecordingWriter *pThis = (CMinimalRecordingWriter *)pUser;
			if(DataSize <= 0)
			{
				return;
			}
			const size_t OldSize = pThis->m_vBuffer.size();
			pThis->m_vBuffer.resize(OldSize + DataSize);
			mem_copy(&pThis->m_vBuffer[OldSize], pData, DataSize);
		}
	};

} // namespace

// Drives a real `CGameContext`/`CGameWorld` (via the same `GameWorld` fixture the golden physics
// tests use) with the inputs from a teehistorian recording, one server tick at a time, comparing
// the resulting character positions against what the recording says they were.
class CReplayWorld : public GameWorld // NOLINT(readability-identifier-naming)
{
public:
	// `GameWorld` is a gtest fixture (`::testing::Test`), which normally gets its `TestBody`
	// generated by `TEST_F`. `CReplayWorld` is instantiated directly instead (once per recording,
	// from inside a single `TEST`), so it needs a trivial one of its own.
	void TestBody() override {}

	CReplayClient m_aClients[MAX_CLIENTS];
	int m_SimTick = -1;
	CPrng m_ReplayPrng;

	/**
	 * Seeds the world's random number generator from the recording's `prng_description`, which
	 * `CPrng::Seed` writes as `pcg-xsh-rr:<seed0>:<seed1>` with both halves in full hex. Teleport
	 * destinations are chosen with `CWorldCore::RandomOr0`, so without the recorded seed a tee
	 * that hits a teleporter with several exits takes a different one than it did originally and
	 * every later position diverges. Returns false if the description is absent or unparsable,
	 * which older recordings may be.
	 */
	bool SeedPrngFrom(const std::string &Description)
	{
		unsigned Seed0Hi, Seed0Lo, Seed1Hi, Seed1Lo;
		if(sscanf(Description.c_str(), "pcg-xsh-rr:%08x%08x:%08x%08x", &Seed0Hi, &Seed0Lo, &Seed1Hi, &Seed1Lo) != 4)
		{
			return false;
		}
		uint64_t aSeed[2];
		aSeed[0] = ((uint64_t)Seed0Hi << 32) | Seed0Lo;
		aSeed[1] = ((uint64_t)Seed1Hi << 32) | Seed1Lo;
		m_ReplayPrng.Seed(aSeed);
		GameServer()->m_World.m_Core.m_pPrng = &m_ReplayPrng;
		return true;
	}

	void EnsurePlayer(int ClientId)
	{
		if(!m_aClients[ClientId].m_Joined)
		{
			GameServer()->CreatePlayer(ClientId, TEAM_GAME, false, -1);
			m_aClients[ClientId].m_Joined = true;
		}
	}

	// Advances the world by exactly one tick, applying the last-known input to every currently
	// alive replayed character first. This mirrors the real server's main loop, which calls
	// `OnClientPredictedEarlyInput`/`OnClientPredictedInput` every tick regardless of whether new
	// input arrived (reusing the last input otherwise), immediately before `OnTick`.
	void SimulateOneTick(int Tick, CReplayReport *pReport)
	{
		// Advance the real server tick in lockstep so tick-gated entities behave as they did
		// (e.g. `CDragger::Tick` only calls `LookForPlayersToDrag` when
		// `Server()->Tick() % (TickSpeed() * 0.15f) == 0`; leaving the tick pinned would mean
		// draggers never acquire a target and every dragged tee stands still through the replay).
		// Set to the tick the recording is describing, not to a counter of our own. A counter
		// only lines up with the recording when the replay happens to start at the same tick,
		// and being one out shifts every dragger evaluation by a tick.
		m_pServer->SetCurrentGameTickForTesting(Tick);
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			if(!m_aClients[ClientId].m_Alive)
				continue;
			CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
			CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(pChr == nullptr)
				continue;
			pChr->OnDirectInput(&m_aClients[ClientId].m_PendingInput);
			pChr->OnPredictedInput(&m_aClients[ClientId].m_PendingInput);
		}
		GameServer()->OnTick();
		pReport->m_TicksReplayed++;
	}

	// Compares every currently alive replayed character's position against the ground truth the
	// recording last established for it. This is valid for `Tick` regardless of whether anything
	// was actually written for `Tick` in the recording: the writer only emits a chunk when a
	// player's position differs from the previous tick's, so "no chunk" means "unchanged".
	void CompareAllAlive(int Tick, CReplayReport *pReport)
	{
		bool AnyDivergenceThisTick = false;

		for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			CReplayClient &Client = m_aClients[ClientId];
			if(!Client.m_Alive)
				continue;
			CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
			CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;

			const int RecordedX = round_to_int(Client.m_RecordedPos.x);
			const int RecordedY = round_to_int(Client.m_RecordedPos.y);
			// A missing character where the recording says one is alive is itself a divergence.
			const int ReplayedX = pChr ? round_to_int(pChr->Core()->m_Pos.x) : RecordedX + 1;
			const int ReplayedY = pChr ? round_to_int(pChr->Core()->m_Pos.y) : RecordedY + 1;

			pReport->m_PositionsCompared++;
			if(!pChr || RecordedX != ReplayedX || RecordedY != ReplayedY)
			{
				pReport->m_DivergentComparisons++;
				if(pReport->m_aDivergentPerClient[ClientId] == 0)
					pReport->m_aFirstDivergentTick[ClientId] = Tick;
				pReport->m_aDivergentPerClient[ClientId]++;
				AnyDivergenceThisTick = true;
				if(pReport->m_vFirstDivergences.size() < 10)
				{
					pReport->m_vFirstDivergences.push_back({Tick, ClientId, RecordedX, RecordedY, ReplayedX, ReplayedY});
				}
			}
		}
		if(AnyDivergenceThisTick)
		{
			pReport->m_DivergentTicks++;
		}
	}

	void ApplyEvent(const CTeeHistorianEvent &Event, CReplayReport *pReport)
	{
		// Not indexed by client id below (and, uniquely among these three, its `m_ClientId` can
		// legitimately be `IConsole::CLIENT_ID_UNSPECIFIED` for a server-console-issued command):
		// count it before the range check that guards every other, genuinely per-client case.
		if(Event.m_Type == TEEHISTORIAN_READER_EVENT_CONSOLE_COMMAND)
		{
			// Only commands that can move, kill or re-team a character make a recording
			// inconclusive. Most recorded commands are administrative (`record`, `status`,
			// `timeout`, `say`) and cannot change physics, so flagging every one would make every
			// real recording inconclusive and the comparison worthless. The list errs towards
			// flagging; anything inert but missing from it only costs strictness on that file.
			static const char *const s_apAffecting[] = {
				"kill", "rescue", "r", "tp", "tpxy", "teleport", "tele", "totele", "totelecp",
				"pause", "spec", "unpause", "practice", "save", "load", "team", "swap",
				"left", "right", "up", "down", "move", "movex", "movey",
				"unsolo", "undeep", "unfreeze", "solo", "deep", "freeze", "ninja", "invincible"};
			for(const char *pAffecting : s_apAffecting)
			{
				if(str_comp_nocase(Event.m_Str.c_str(), pAffecting) == 0)
				{
					pReport->m_NotReplayedEvents++;
					break;
				}
			}
			return;
		}
		if(Event.m_ClientId < 0 || Event.m_ClientId >= MAX_CLIENTS)
		{
			return;
		}
		CReplayClient &Client = m_aClients[Event.m_ClientId];
		switch(Event.m_Type)
		{
		case TEEHISTORIAN_READER_EVENT_JOIN:
			EnsurePlayer(Event.m_ClientId);
			break;

		case TEEHISTORIAN_READER_EVENT_DROP:
			if(Client.m_Alive)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[Event.m_ClientId];
				if(pPlayer)
					pPlayer->KillCharacter();
			}
			Client.m_Alive = false;
			Client.m_Joined = false;
			break;

		case TEEHISTORIAN_READER_EVENT_PLAYER_NEW:
		{
			EnsurePlayer(Event.m_ClientId);
			CPlayer *pPlayer = GameServer()->m_apPlayers[Event.m_ClientId];
			const vec2 Pos = vec2((float)Event.m_X, (float)Event.m_Y);
			pPlayer->ForceSpawn(Pos);
			Client.m_Alive = true;
			Client.m_RecordedPos = Pos;
			break;
		}

		case TEEHISTORIAN_READER_EVENT_PLAYER_DIFF:
		{
			const vec2 Pos = vec2((float)Event.m_X, (float)Event.m_Y);
			if(!Client.m_Alive)
			{
				// The recording started mid-session: this client was already alive when
				// recording began, so there was no PLAYER_NEW establishing their spawn.
				EnsurePlayer(Event.m_ClientId);
				CPlayer *pPlayer = GameServer()->m_apPlayers[Event.m_ClientId];
				pPlayer->ForceSpawn(Pos);
				Client.m_Alive = true;
				pReport->m_ImplicitSpawns++;
			}
			Client.m_RecordedPos = Pos;
			break;
		}

		case TEEHISTORIAN_READER_EVENT_PLAYER_OLD:
			if(Client.m_Alive)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[Event.m_ClientId];
				if(pPlayer)
					pPlayer->KillCharacter();
			}
			Client.m_Alive = false;
			break;

		case TEEHISTORIAN_READER_EVENT_INPUT_NEW:
		case TEEHISTORIAN_READER_EVENT_INPUT_DIFF:
			Client.m_PendingInput = Event.m_Input;
			break;

		case TEEHISTORIAN_READER_EVENT_MESSAGE:
		case TEEHISTORIAN_READER_EVENT_PLAYER_TEAM:
			// Not replayed, but not inert either: see `CReplayReport::m_NotReplayedEvents`.
			// (CONSOLE_COMMAND is handled above, before the client id range check.)
			pReport->m_NotReplayedEvents++;
			break;

		default:
			// Genuinely inert for physics: TICK_SKIP is handled purely via `Event.m_Tick` by the
			// caller, and the rest (ready state, auth, antibot, finishes, saves/loads, protocol
			// version, ...) never touches a character's position.
			break;
		}
	}

	// Replays every event in `vFile` against a fresh world, tick by tick, and reports how the
	// resulting positions compared to the ones the recording says actually happened.
	CReplayReport Replay(const std::vector<unsigned char> &vFile)
	{
		CReplayReport Report;

		CTeeHistorianReader Reader;
		char aError[256];
		if(!Reader.Open(vFile.data(), vFile.size(), aError, sizeof(aError)))
		{
			Report.m_ReaderError = true;
			Report.m_ReaderErrorMessage = aError;
			return Report;
		}

		if(Reader.Header().m_MapName != "coverage")
		{
			char aReason[256];
			str_format(aReason, sizeof(aReason), "recording is for map '%s', fixture loaded 'coverage'", Reader.Header().m_MapName.c_str());
			Report.m_MapMismatchReason = aReason;
			return Report;
		}

		Report.m_PrngSeeded = SeedPrngFrom(Reader.Header().m_PrngDescription);

		CTeeHistorianEvent Event;
		bool HaveEvent = Reader.NextEvent(&Event);
		while(HaveEvent)
		{
			const int Tick = Event.m_Tick;
			if(Tick - m_SimTick > MAX_TICK_GAP)
			{
				Report.m_AbortedTickGapTooLarge = true;
				Report.m_AbortedAtTick = Tick;
				return Report;
			}
			// Ticks strictly between the last one we simulated and this event's tick had no
			// recorded change at all: simulate and compare them using the carried-forward ground
			// truth before touching anything this event says.
			for(int T = m_SimTick + 1; T < Tick; T++)
			{
				SimulateOneTick(T, &Report);
				CompareAllAlive(T, &Report);
			}
			if(Tick > m_SimTick)
			{
				SimulateOneTick(Tick, &Report);
				m_SimTick = Tick;
			}

			ApplyEvent(Event, &Report);

			// Multiple events can share the same tick (e.g. two clients' positions both changed).
			// Only compare once every event for this tick has been applied.
			CTeeHistorianEvent NextEvent;
			const bool HaveNext = Reader.NextEvent(&NextEvent);
			if(!HaveNext || NextEvent.m_Tick != Tick)
			{
				CompareAllAlive(Tick, &Report);
			}
			Event = NextEvent;
			HaveEvent = HaveNext;
		}

		Report.m_ReaderError = Reader.Error();
		Report.m_ReaderErrorMessage = Reader.ErrorMessage();
		return Report;
	}
};

// Replays every *.teehistorian recording found in `DDNET_TEEHISTORIAN_TEST_DIR` (an additional,
// opt-in local corpus check; unlike `TeeHistorianReplay.SelfRecording` below, this one is skipped
// by default, same opt-in as `TeeHistorianReader.RealRecordings`) against a fresh `coverage` world
// and compares the resulting positions against what the recording says actually happened.
//
// A recording containing any event this harness doesn't apply to physics (`ApplyEvent`'s
// CONSOLE_COMMAND/MESSAGE/PLAYER_TEAM cases -- see `CReplayReport::m_NotReplayedEvents`) is
// reported inconclusive rather than compared: any divergence, or lack of one, could equally be
// explained by the event this harness skipped. For every other recording, comparisons never use a
// tolerance -- positions are effectively integers (`round_to_int`, matching how the recording
// itself rounds them), so they either match exactly or they tell you something, and zero
// divergence is asserted.
TEST(TeeHistorianReplay, RealRecordings)
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
	std::sort(vNames.begin(), vNames.end());

	ASSERT_FALSE(vNames.empty()) << "no *.teehistorian files in " << pDir;

	bool AnyComparisonMade = false;

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

		std::vector<unsigned char> vFile((unsigned char *)pData, (unsigned char *)pData + DataSize);

		CReplayWorld World;
		CReplayReport Report = World.Replay(vFile);

		if(!Report.m_MapMismatchReason.empty())
		{
			printf("%s: skipped, %s\n", aPath, Report.m_MapMismatchReason.c_str());
			continue;
		}

		EXPECT_FALSE(Report.m_ReaderError) << aPath << ": " << Report.m_ReaderErrorMessage;
		if(Report.m_ReaderError)
		{
			continue;
		}

		ASSERT_FALSE(Report.m_AbortedTickGapTooLarge)
			<< aPath << ": gap exceeding " << MAX_TICK_GAP << " ticks before tick " << Report.m_AbortedAtTick
			<< "; recording is corrupt or hostile";

		printf("%s: %d ticks replayed, %d positions compared, %d divergent comparison(s) across %d tick(s)%s\n",
			aPath, Report.m_TicksReplayed, Report.m_PositionsCompared, Report.m_DivergentComparisons,
			Report.m_DivergentTicks, Report.m_ImplicitSpawns ? " (implicit spawn(s) seen)" : "");
		for(int c = 0; c < MAX_CLIENTS; c++)
			if(Report.m_aDivergentPerClient[c])
				printf("%s:   client %d: %d divergent comparison(s), first at tick %d\n", aPath, c, Report.m_aDivergentPerClient[c], Report.m_aFirstDivergentTick[c]);

		if(!Report.m_vFirstDivergences.empty())
		{
			printf("%s: first %zu divergent comparison(s):\n", aPath, Report.m_vFirstDivergences.size());
			for(const CDivergence &D : Report.m_vFirstDivergences)
			{
				printf("%s:   tick=%d client=%d recorded=(%d,%d) replayed=(%d,%d)\n",
					aPath, D.m_Tick, D.m_ClientId, D.m_RecordedX, D.m_RecordedY, D.m_ReplayedX, D.m_ReplayedY);
			}
		}

		if(Report.m_PositionsCompared == 0)
		{
			printf("%s: no player ever appeared alive in this recording, nothing to compare\n", aPath);
			continue;
		}
		AnyComparisonMade = true;

		if(Report.m_NotReplayedEvents > 0)
		{
			printf("%s: inconclusive, %d event(s) not replayed (chat/rcon/team assignment can move a character)\n",
				aPath, Report.m_NotReplayedEvents);
			continue;
		}

		EXPECT_EQ(Report.m_DivergentComparisons, 0) << aPath << ": replayed physics diverge from the recording (see stdout above for the full per-tick findings)";
	}

	ASSERT_TRUE(AnyComparisonMade) << "no recording in " << pDir << " produced a character to compare";
}

namespace
{

	// Drives a real fixture instance with `sv_tee_historian` on and a scripted movement, so
	// `TeeHistorianReplay.SelfRecording` has a real recording to replay without depending on an
	// external corpus. Routes input through `CGameContext::OnClientPredictedEarlyInput`/
	// `OnClientPredictedInput`, the same entry points `CServer`'s real per-tick loop calls, rather
	// than calling the character directly (the way `GoldenPhysics::Tick` in gameworld_test.cpp
	// does): only `OnClientPredictedEarlyInput` also records the input into the historian, so
	// calling the character directly would produce a recording with position changes but no
	// INPUT_NEW/INPUT_DIFF events to explain them.
	class CRecorderWorld : public GameWorld // NOLINT(readability-identifier-naming)
	{
	public:
		CRecorderWorld() :
			GameWorld(/*EnableTeeHistorian=*/true)
		{
		}

		void TestBody() override {}

		void RecordMovement()
		{
			GameServer()->CreatePlayer(0, TEAM_GAME, false, -1);
			CPlayer *pPlayer = GameServer()->m_apPlayers[0];
			// Already resting on the ground (the landing spot `GoldenPhysics.FallsAndLands` in
			// gameworld_test.cpp settles at from a fall), not mid-air: a PLAYER_NEW event only
			// records a position, never a velocity, so `CReplayWorld::ApplyEvent` reconstructs a
			// spawn as velocity zero. That's exactly what a resting spawn has, but a mid-air one
			// doesn't -- there the recorded position already has a tick of unrecorded fall
			// velocity baked in that force-spawning-from-position-only can't reproduce, and the
			// replay falls a little slower than the recording from that point on.
			pPlayer->ForceSpawn(vec2(1000.0f, 305.0f));

			CNetObj_PlayerInput NoInput = {};
			RunTicks(NoInput, 5); // a few ticks resting, to prove standing still round-trips too

			CNetObj_PlayerInput WalkInput = {};
			WalkInput.m_Direction = 1;
			RunTicks(WalkInput, 40); // walk right

			CNetObj_PlayerInput JumpInput = {};
			JumpInput.m_Jump = 1;
			RunTicks(JumpInput, 40); // jump
		}

	private:
		void RunTicks(const CNetObj_PlayerInput &Input, int Ticks)
		{
			// Mirrors the ordering in `CServer::Run`: early input at the old tick, then the tick
			// counter advances, then input and `OnTick` at the new one. `CTeeHistorian::BeginTick`
			// keys off `Server()->Tick()`, so leaving it pinned would make every tick collapse
			// onto the same one and eventually produce a negative delta once a real TICK_SKIP is
			// due -- the exact "negative tick skip delta" error `CTeeHistorianReader` now rejects.
			for(int i = 0; i < Ticks; i++)
			{
				GameServer()->OnClientPredictedEarlyInput(0, &Input);
				m_pServer->SetCurrentGameTickForTesting(m_pServer->Tick() + 1);
				GameServer()->OnClientPredictedInput(0, &Input);
				GameServer()->OnTick();
			}
		}
	};

} // namespace

// Enables `sv_tee_historian` on a real fixture instance (`CGameContext` owns the historian and
// writes it through the fixture's own temp storage, see `gamecontext.h`'s `m_TeeHistorian`/
// `TeeHistorian()`/`m_TeeHistorianActive`), drives it with a scripted movement, finishes the
// recording, and replays the exact bytes the server itself just wrote back through `CReplayWorld`.
// Unlike `RealRecordings` above, this needs no external corpus and no environment variable, so it
// runs unconditionally in CI; no `.teehistorian` file is committed to the repo or embedded as a
// byte array; the only one that ever exists is produced by this test at runtime, in a temp
// directory the fixture's own `CTestInfo` tears down afterwards.
TEST(TeeHistorianReplay, SelfRecording)
{
	std::vector<unsigned char> vFile;
	{
		CRecorderWorld Recorder;
		Recorder.RecordMovement();
		// Finishes and flushes the recording to disk before the fixture (and its temp storage)
		// go away; see `GameWorld::ShutdownGameServer`.
		Recorder.ShutdownGameServer();

		std::vector<std::string> vNames;
		Recorder.m_pStorage->ListDirectory(
			IStorage::TYPE_SAVE, "teehistorian",
			[](const char *pName, int IsDir, int, void *pUser) -> int {
				if(!IsDir && str_endswith(pName, ".teehistorian"))
				{
					((std::vector<std::string> *)pUser)->emplace_back(pName);
				}
				return 0;
			},
			&vNames);
		ASSERT_EQ(vNames.size(), 1u);

		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "teehistorian/%s", vNames[0].c_str());
		void *pData;
		unsigned DataSize;
		ASSERT_TRUE(Recorder.m_pStorage->ReadFile(aPath, IStorage::TYPE_SAVE, &pData, &DataSize)) << aPath;
		std::unique_ptr<void, decltype(&free)> pDataGuard(pData, free);
		vFile.assign((unsigned char *)pData, (unsigned char *)pData + DataSize);
	}

	CReplayWorld World;
	CReplayReport Report = World.Replay(vFile);

	ASSERT_TRUE(Report.m_MapMismatchReason.empty()) << Report.m_MapMismatchReason;
	ASSERT_FALSE(Report.m_ReaderError) << Report.m_ReaderErrorMessage;
	ASSERT_FALSE(Report.m_AbortedTickGapTooLarge);
	// The scripted movement above issues no chat/rcon/team-assignment events.
	ASSERT_EQ(Report.m_NotReplayedEvents, 0);
	ASSERT_GT(Report.m_PositionsCompared, 0);
	EXPECT_EQ(Report.m_DivergentComparisons, 0);
}

// A `TICK_SKIP` delta well under `CTeeHistorianReader`'s own implausible-tick bound (so the
// reader accepts it) but well over `MAX_TICK_GAP` must make the harness abort that recording
// instead of spending one real `OnTick()` call per simulated tick trying to catch up -- the
// "corrupt tick delta hangs the replay" finding this guards against, without actually needing to
// wait out a multi-hour hang to prove it (`ASSERT_FALSE`s on the report, not on a timeout).
TEST(TeeHistorianReplay, AbortsOnImplausibleTickGap)
{
	CMinimalRecordingWriter Writer;

	CNetObj_CharacterCore Char = {};
	Char.m_X = 1000;
	Char.m_Y = 305;

	Writer.m_TH.BeginTick(1);
	Writer.m_TH.BeginPlayers();
	Writer.m_TH.RecordPlayer(0, &Char);
	Writer.m_TH.EndPlayers();
	Writer.m_TH.BeginInputs();
	Writer.m_TH.EndInputs();
	Writer.m_TH.EndTick();

	// Far past `MAX_TICK_GAP` (10,000), far short of `CTeeHistorianReader`'s implausible-tick
	// bound (1,000,000,000): the reader must accept this, only the harness should refuse it.
	constexpr int HugeTick = 50'000;
	Char.m_X = 1001; // force a chunk to actually be written for this tick
	Writer.m_TH.BeginTick(HugeTick);
	Writer.m_TH.BeginPlayers();
	Writer.m_TH.RecordPlayer(0, &Char);
	Writer.m_TH.EndPlayers();
	Writer.m_TH.BeginInputs();
	Writer.m_TH.EndInputs();
	Writer.m_TH.EndTick();
	Writer.m_TH.Finish();

	CReplayWorld World;
	CReplayReport Report = World.Replay(Writer.m_vBuffer);

	ASSERT_TRUE(Report.m_MapMismatchReason.empty()) << Report.m_MapMismatchReason;
	ASSERT_FALSE(Report.m_ReaderError) << Report.m_ReaderErrorMessage;
	EXPECT_TRUE(Report.m_AbortedTickGapTooLarge);
	EXPECT_EQ(Report.m_AbortedAtTick, HugeTick);
	EXPECT_LT(Report.m_TicksReplayed, MAX_TICK_GAP) << "the harness ran ahead instead of aborting";
}
