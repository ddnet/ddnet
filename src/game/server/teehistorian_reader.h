#ifndef GAME_SERVER_TEEHISTORIAN_READER_H
#define GAME_SERVER_TEEHISTORIAN_READER_H

#include <base/hash.h>

#include <engine/shared/protocol.h>
#include <engine/shared/uuid_manager.h>

#include <generated/protocol.h>

#include <game/gamecore.h>

#include <cstdint>
#include <string>
#include <vector>

// Events produced by `CTeeHistorianReader::NextEvent`. Every event carries
// the tick it belongs to; only the fields documented for that type are
// meaningful, the rest are default-initialized.
enum ETeeHistorianReaderEvent
{
	// A tick boundary, either because a TICK_SKIP chunk was present on disk
	// (`m_TickIsExplicit == true`, `m_TickDelta` holds the decoded delta) or
	// because it was inferred from player-data ordering the way
	// `CTeeHistorian::EnsureTickWrittenPlayerData` does on the write side.
	TEEHISTORIAN_READER_EVENT_TICK_SKIP,
	// A player appeared or moved for the first time since becoming dead/unseen.
	// `m_ClientId`, `m_X`, `m_Y` (absolute).
	TEEHISTORIAN_READER_EVENT_PLAYER_NEW,
	// A player was removed/died. `m_ClientId`.
	TEEHISTORIAN_READER_EVENT_PLAYER_OLD,
	// A player moved while already alive. `m_ClientId`, `m_X`, `m_Y` (absolute, decoded from a delta).
	TEEHISTORIAN_READER_EVENT_PLAYER_DIFF,
	// A player's full input state. `m_ClientId`, `m_Input` (absolute).
	TEEHISTORIAN_READER_EVENT_INPUT_NEW,
	// A player's input state, decoded from a delta against the previous one. `m_ClientId`, `m_Input` (absolute).
	TEEHISTORIAN_READER_EVENT_INPUT_DIFF,
	// A raw network message sent by a player. `m_ClientId`, `m_vRawData`.
	TEEHISTORIAN_READER_EVENT_MESSAGE,
	// A player joined. `m_ClientId`.
	TEEHISTORIAN_READER_EVENT_JOIN,
	// A player left. `m_ClientId`, `m_aStr[0]` (reason).
	TEEHISTORIAN_READER_EVENT_DROP,
	// A console command was executed. `m_ClientId`, `m_FlagMask`, `m_aStr[0]` (command), `m_vArgs`.
	TEEHISTORIAN_READER_EVENT_CONSOLE_COMMAND,
	// End of the recording. No further events follow.
	TEEHISTORIAN_READER_EVENT_FINISH,
	// DDNet team assignment changed. `m_ClientId`, `m_Team`.
	TEEHISTORIAN_READER_EVENT_PLAYER_TEAM,
	// A team's practice mode changed. `m_Team`, `m_Practice`.
	TEEHISTORIAN_READER_EVENT_TEAM_PRACTICE,
	// A player pressed ready. `m_ClientId`.
	TEEHISTORIAN_READER_EVENT_PLAYER_READY,
	// A player rejoined (protocol 6 spectator-to-player transition). `m_ClientId`.
	TEEHISTORIAN_READER_EVENT_PLAYER_REJOIN,
	// Opaque antibot data. `m_vRawData`.
	TEEHISTORIAN_READER_EVENT_ANTIBOT,
	// A player's name changed/was recorded. `m_ClientId`, `m_aStr[0]` (name).
	TEEHISTORIAN_READER_EVENT_PLAYER_NAME,
	// A player finished the map. `m_ClientId`, `m_TimeTicks`.
	TEEHISTORIAN_READER_EVENT_PLAYER_FINISH,
	// A team finished the map. `m_Team`, `m_TimeTicks`.
	TEEHISTORIAN_READER_EVENT_TEAM_FINISH,
	// Two players swapped. `m_ClientId`, `m_ClientId2`.
	TEEHISTORIAN_READER_EVENT_PLAYER_SWITCH,
	// A team save succeeded. `m_Team`, `m_SaveId`, `m_aStr[0]` (team save string).
	TEEHISTORIAN_READER_EVENT_SAVE_SUCCESS,
	// A team save failed. `m_Team`.
	TEEHISTORIAN_READER_EVENT_SAVE_FAILURE,
	// A team load succeeded. `m_Team`, `m_SaveId`, `m_aStr[0]` (team save string).
	TEEHISTORIAN_READER_EVENT_LOAD_SUCCESS,
	// A team load failed. `m_Team`.
	TEEHISTORIAN_READER_EVENT_LOAD_FAILURE,
	// A DDNet client connected. `m_ClientId`, `m_ConnectionId`, `m_DDNetVersion`, `m_aStr[0]` (version string).
	TEEHISTORIAN_READER_EVENT_DDNETVER,
	// A DDNet client connected (legacy, no connection id/version string). `m_ClientId`, `m_DDNetVersion`.
	TEEHISTORIAN_READER_EVENT_DDNETVER_OLD,
	// A player's initial auth level was recorded. `m_ClientId`, `m_AuthLevel`, `m_aStr[0]` (auth name).
	TEEHISTORIAN_READER_EVENT_AUTH_INIT,
	// A player logged in. `m_ClientId`, `m_AuthLevel`, `m_aStr[0]` (auth name).
	TEEHISTORIAN_READER_EVENT_AUTH_LOGIN,
	// A player logged out. `m_ClientId`.
	TEEHISTORIAN_READER_EVENT_AUTH_LOGOUT,
	// A protocol 0.6 player joined. `m_ClientId`.
	TEEHISTORIAN_READER_EVENT_JOINVER6,
	// A protocol 0.7 player joined. `m_ClientId`.
	TEEHISTORIAN_READER_EVENT_JOINVER7,
	// Debug marker with no payload.
	TEEHISTORIAN_READER_EVENT_TEST,
	// A `TEEHISTORIAN_EX` chunk with a UUID this reader does not know about.
	// `m_UnknownUuid`, `m_vRawData` (the chunk's raw payload, safely skipped).
	TEEHISTORIAN_READER_EVENT_UNKNOWN_EX,
};

// A single decoded event. Only the fields documented on `m_Type`'s enumerator
// are populated; all others are left at their default value.
struct CTeeHistorianEvent
{
	ETeeHistorianReaderEvent m_Type;
	int m_Tick = 0;

	// TICK_SKIP only.
	bool m_TickIsExplicit = false;
	int m_TickDelta = 0;

	int m_ClientId = -1;
	int m_ClientId2 = -1;
	int m_X = 0;
	int m_Y = 0;
	int m_Team = 0;
	bool m_Practice = false;
	CNetObj_PlayerInput m_Input = {};
	int m_FlagMask = 0;
	int m_Protocol = 0;
	int m_DDNetVersion = 0;
	int m_AuthLevel = 0;
	int m_TimeTicks = 0;
	CUuid m_ConnectionId = UUID_ZEROED;
	CUuid m_SaveId = UUID_ZEROED;
	CUuid m_UnknownUuid = UUID_ZEROED;
	// Generic string slots, meaning depends on `m_Type` (see enum documentation).
	std::string m_aStr[1];
	std::vector<std::string> m_vArgs;
	std::vector<unsigned char> m_vRawData;
};

// Parsed JSON header written by `CTeeHistorian::WriteHeader`.
struct CTeeHistorianHeader
{
	int m_Version = 0;
	int m_VersionMinor = 0;
	CUuid m_GameUuid = UUID_ZEROED;
	bool m_HavePrevGameUuid = false;
	CUuid m_PrevGameUuid = UUID_ZEROED;
	std::string m_aServerVersion;
	std::string m_aStartTime;
	std::string m_aServerName;
	int m_ServerPort = 0;
	std::string m_aGameType;
	std::string m_aMapName;
	int m_MapSize = 0;
	SHA256_DIGEST m_MapSha256 = {};
	int m_MapCrc = 0;
	std::string m_aPrngDescription;
	// Tuning as written in the "tuning" JSON object, overlaid on top of
	// `CTuningParams::DEFAULT`. Present even when the header has no tuning
	// overrides at all (then it just equals the defaults).
	CTuningParams m_Tuning = CTuningParams::DEFAULT;
};

// Streaming reader for the teehistorian binary format written by
// `CTeeHistorian` (see `src/game/server/teehistorian.cpp`, the authoritative
// writer this class mirrors).
//
// Unknown `TEEHISTORIAN_EX` UUIDs are skipped (their chunks are
// length-prefixed, so this is always safe) and reported via
// `UnknownExUuids()`/`NumUnknownExChunks()`. Unknown *top-level* message
// types can not be skipped safely (the format gives them no length prefix,
// unlike EX chunks) and are treated as a parse error.
class CTeeHistorianReader
{
public:
	CTeeHistorianReader();

	// Parses the UUID magic and JSON header from `pData`/`Size`. Returns
	// false and fills `paError` (must hold at least `ErrorSize` bytes) on
	// failure. Never crashes or asserts on malformed input.
	bool Open(const void *pData, size_t Size, char *pError, size_t ErrorSize);

	const CTeeHistorianHeader &Header() const { return m_Header; }

	// Decodes the next event from the body. Returns false when the stream
	// ends (either cleanly, after a FINISH event, or due to a parse error;
	// use `Error()` to distinguish the two) or once `pOutError` was already
	// set by a previous call.
	bool NextEvent(CTeeHistorianEvent *pEvent);

	bool Error() const { return m_Error; }
	const char *ErrorMessage() const { return m_aError; }

	// True once a FINISH event has been returned by `NextEvent`.
	bool Finished() const { return m_Finished; }
	// Number of bytes left in the body after the FINISH event (0 if the
	// stream hasn't finished yet, or ended exactly at FINISH).
	size_t UnconsumedBytesAfterFinish() const;

	// True if there is no more data to read. Can be true without `Finished()`
	// being true: a recording can legitimately end (e.g. the server crashed)
	// without ever writing a FINISH chunk; that is not by itself a parse
	// error, just an incomplete recording.
	bool AtEndOfBuffer() const { return m_Pos >= m_Size; }

	// Distinct unknown TEEHISTORIAN_EX UUIDs encountered so far, and how
	// many chunks of each were skipped.
	const std::vector<std::pair<CUuid, int>> &UnknownExUuids() const { return m_vUnknownExUuids; }
	int NumUnknownExChunks() const { return m_NumUnknownExChunks; }

private:
	struct CPlayerState
	{
		bool m_Alive = false;
		int m_X = 0;
		int m_Y = 0;
	};
	struct CInputState
	{
		bool m_Have = false;
		CNetObj_PlayerInput m_Input = {};
	};

	bool ReadInt(int *pOut);
	const unsigned char *ReadRaw(size_t Size);
	bool ReadString(std::string *pOut);
	bool ReadUuid(CUuid *pOut);

	void SetError(const char *pMsg);
	bool AtEnd() const { return m_Pos >= m_Size; }

	// Attaches the current tick to `pEvent` and, if a new tick starts
	// implicitly at ClientId `ClientId`, queues a TICK_SKIP event ahead of it.
	// Returns false (and sets an error) if ClientId is out of range.
	bool AdvancePlayerDataTick(int ClientId);
	void QueueOrReturn(CTeeHistorianEvent *pOutEvent, const CTeeHistorianEvent &DataEvent);

	bool HandleEx(CTeeHistorianEvent *pEvent);

	const unsigned char *m_pData = nullptr;
	size_t m_Size = 0;
	size_t m_Pos = 0;

	bool m_Error = false;
	char m_aError[256] = {0};
	bool m_Finished = false;

	CTeeHistorianHeader m_Header;

	// Mirrors `CTeeHistorian`'s private tick/ordering state.
	int m_CurrentTick = 0;
	int m_LastWrittenTick = 0;
	int m_RunningMaxClientId = MAX_CLIENTS;

	CPlayerState m_aPlayers[MAX_CLIENTS];
	CInputState m_aInputs[MAX_CLIENTS];

	std::vector<CTeeHistorianEvent> m_QueuedEvents;

	std::vector<std::pair<CUuid, int>> m_vUnknownExUuids;
	int m_NumUnknownExChunks = 0;
};

#endif // GAME_SERVER_TEEHISTORIAN_READER_H
