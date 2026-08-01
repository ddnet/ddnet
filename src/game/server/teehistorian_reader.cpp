#include "teehistorian_reader.h"

#include <base/mem.h>
#include <base/str.h>

#include <engine/console.h>
#include <engine/shared/compression.h>
#include <engine/shared/json.h>
#include <engine/shared/teehistorian_opcodes.h>

#include <cstring>
#include <iterator>

#define UUID(id, name) static const CUuid UUID_##id = CalculateUuid(name);
#include <engine/shared/teehistorian_ex_chunks.h>
#undef UUID

// One enumerator per `UUID()` entry in teehistorian_ex_chunks.h, in file order, plus a
// matching `CUuid` table built from the same X-macro so the two can't drift apart. `HandleEx`
// looks up a chunk's index in `s_aExChunkUuids` and switches on it as this enum, with no
// `default` label: adding a `UUID()` to teehistorian_ex_chunks.h with no matching `case`
// leaves that switch non-exhaustive, which is a `-Wswitch` build error (the project builds
// with `-Werror`), the same drift protection the previous `goto found_##id` ladder gave.
enum ETeeHistorianExChunk
{
#define UUID(id, name) EX_##id,
#include <engine/shared/teehistorian_ex_chunks.h>
#undef UUID
};

static const CUuid s_aExChunkUuids[] = {
#define UUID(id, name) UUID_##id,
#include <engine/shared/teehistorian_ex_chunks.h>
#undef UUID
};
static constexpr size_t NUM_TEEHISTORIAN_EX_CHUNKS = std::size(s_aExChunkUuids);

static const char TEEHISTORIAN_NAME[] = "teehistorian@ddnet.tw";
static const CUuid TEEHISTORIAN_UUID = CalculateUuid(TEEHISTORIAN_NAME);
// Mirrors `TEEHISTORIAN_VERSION` in teehistorian.cpp: the only body format this reader
// understands. Not exposed by the writer, so re-declared here.
static const int TEEHISTORIAN_VERSION = 2;

// Far more than any real DDNet server, restarted at the very least on every map change, could
// ever reach at the fixed 50 ticks/second server tick rate (~231 days of continuous uptime),
// but far enough below `INT_MAX` (~2^31) that adding a small, legitimate delta to it (a
// following TICK_SKIP, or `AdvancePlayerDataTick`'s implicit +1) can never itself overflow.
// Bounding the *result* of a tick update against this catches a corrupt/hostile absolute tick
// (e.g. a multi-billion-tick TICK_SKIP delta) before it either overflows or turns into an
// attempted multi-billion-tick simulation by a consumer replaying this reader's events.
static constexpr int64_t MAX_PLAUSIBLE_TICK = 1'000'000'000;

CTeeHistorianReader::CTeeHistorianReader() = default;

void CTeeHistorianReader::SetError(const char *pMsg)
{
	if(m_Error)
	{
		return;
	}
	m_Error = true;
	str_copy(m_aError, pMsg, sizeof(m_aError));
}

bool CTeeHistorianReader::ReadInt(int *pOut)
{
	if(m_Error)
	{
		return false;
	}
	if(AtEnd())
	{
		SetError("unexpected end of file while reading int");
		return false;
	}
	const unsigned char *pNext = CVariableInt::Unpack(m_pData + m_Pos, pOut, m_Size - m_Pos);
	if(!pNext)
	{
		SetError("malformed variable-length int");
		return false;
	}
	m_Pos = pNext - m_pData;
	return true;
}

const unsigned char *CTeeHistorianReader::ReadRaw(size_t Size)
{
	if(m_Error)
	{
		return nullptr;
	}
	if(Size > m_Size - m_Pos)
	{
		SetError("unexpected end of file while reading raw data");
		return nullptr;
	}
	const unsigned char *pResult = m_pData + m_Pos;
	m_Pos += Size;
	return pResult;
}

bool CTeeHistorianReader::ReadString(std::string *pOut)
{
	if(m_Error)
	{
		return false;
	}
	const unsigned char *pStart = m_pData + m_Pos;
	size_t Len = 0;
	while(true)
	{
		if(m_Pos + Len >= m_Size)
		{
			SetError("unterminated string");
			return false;
		}
		if(pStart[Len] == '\0')
		{
			break;
		}
		Len++;
	}
	pOut->assign((const char *)pStart, Len);
	m_Pos += Len + 1;
	return true;
}

bool CTeeHistorianReader::ReadUuid(CUuid *pOut)
{
	const unsigned char *pRaw = ReadRaw(sizeof(CUuid));
	if(!pRaw)
	{
		return false;
	}
	mem_copy(pOut, pRaw, sizeof(CUuid));
	return true;
}

bool CTeeHistorianReader::ReadClientId(int *pOut)
{
	if(!ReadInt(pOut))
	{
		return false;
	}
	if(*pOut < 0 || *pOut >= MAX_CLIENTS)
	{
		SetError("client id out of range");
		return false;
	}
	return true;
}

bool CTeeHistorianReader::ReadTeamId(int *pOut)
{
	if(!ReadInt(pOut))
	{
		return false;
	}
	if(*pOut < 0 || *pOut >= MAX_CLIENTS)
	{
		SetError("team id out of range");
		return false;
	}
	return true;
}

bool CTeeHistorianReader::ReadClientIdOrUnspecified(int *pOut)
{
	if(!ReadInt(pOut))
	{
		return false;
	}
	if(*pOut == IConsole::CLIENT_ID_UNSPECIFIED)
	{
		return true;
	}
	if(*pOut < 0 || *pOut >= MAX_CLIENTS)
	{
		SetError("client id out of range");
		return false;
	}
	return true;
}

static bool JsonGetStr(const json_value &Value, const char *pKey, std::string *pOut)
{
	const json_value &Field = Value[pKey];
	if(Field.type != json_string)
	{
		return false;
	}
	*pOut = json_string_get(&Field);
	return true;
}

static bool JsonGetIntFromStr(const json_value &Value, const char *pKey, int *pOut, int Base = 10)
{
	const json_value &Field = Value[pKey];
	if(Field.type != json_string)
	{
		return false;
	}
	*pOut = str_toint_base(json_string_get(&Field), Base);
	return true;
}

bool CTeeHistorianReader::Open(const void *pData, size_t Size, char *pError, size_t ErrorSize)
{
	const unsigned char *pBytes = (const unsigned char *)pData;

	if(Size < sizeof(CUuid))
	{
		str_copy(pError, "file too short for teehistorian magic", ErrorSize);
		return false;
	}
	CUuid Magic;
	mem_copy(&Magic, pBytes, sizeof(Magic));
	if(Magic != TEEHISTORIAN_UUID)
	{
		str_copy(pError, "not a teehistorian file (magic mismatch)", ErrorSize);
		return false;
	}

	const char *pJsonStart = (const char *)pBytes + sizeof(CUuid);
	const unsigned char *pNul = (const unsigned char *)memchr(pJsonStart, '\0', Size - sizeof(CUuid));
	if(!pNul)
	{
		str_copy(pError, "unterminated header JSON", ErrorSize);
		return false;
	}
	const size_t JsonLen = pNul - (const unsigned char *)pJsonStart;

	json_value *pJson = JsonParse(pJsonStart, JsonLen);
	if(!pJson)
	{
		str_copy(pError, "invalid header JSON", ErrorSize);
		return false;
	}

	bool Ok = true;
	Ok = Ok && JsonGetIntFromStr(*pJson, "version", &m_Header.m_Version);
	Ok = Ok && JsonGetIntFromStr(*pJson, "version_minor", &m_Header.m_VersionMinor);
	std::string aGameUuid;
	Ok = Ok && JsonGetStr(*pJson, "game_uuid", &aGameUuid);
	Ok = Ok && ParseUuid(&m_Header.m_GameUuid, aGameUuid.c_str()) == 0;
	Ok = Ok && JsonGetStr(*pJson, "server_version", &m_Header.m_ServerVersion);
	Ok = Ok && JsonGetStr(*pJson, "start_time", &m_Header.m_StartTime);
	Ok = Ok && JsonGetStr(*pJson, "server_name", &m_Header.m_ServerName);
	Ok = Ok && JsonGetIntFromStr(*pJson, "server_port", &m_Header.m_ServerPort);
	Ok = Ok && JsonGetStr(*pJson, "game_type", &m_Header.m_GameType);
	Ok = Ok && JsonGetStr(*pJson, "map_name", &m_Header.m_MapName);
	Ok = Ok && JsonGetIntFromStr(*pJson, "map_size", &m_Header.m_MapSize);
	std::string aMapSha256;
	Ok = Ok && JsonGetStr(*pJson, "map_sha256", &aMapSha256);
	Ok = Ok && sha256_from_str(&m_Header.m_MapSha256, aMapSha256.c_str()) == 0;
	Ok = Ok && JsonGetIntFromStr(*pJson, "map_crc", &m_Header.m_MapCrc, 16);
	// `prng_description` was added after the initial format revisions; treat it as optional.
	JsonGetStr(*pJson, "prng_description", &m_Header.m_PrngDescription);

	std::string aPrevGameUuid;
	if(JsonGetStr(*pJson, "prev_game_uuid", &aPrevGameUuid))
	{
		m_Header.m_HavePrevGameUuid = ParseUuid(&m_Header.m_PrevGameUuid, aPrevGameUuid.c_str()) == 0;
		Ok = Ok && m_Header.m_HavePrevGameUuid;
	}

	if(!Ok)
	{
		str_copy(pError, "missing or malformed field in header JSON", ErrorSize);
		json_value_free(pJson);
		return false;
	}

	if(m_Header.m_Version != TEEHISTORIAN_VERSION)
	{
		str_format(pError, ErrorSize, "unsupported teehistorian version %d (only %d is understood)", m_Header.m_Version, TEEHISTORIAN_VERSION);
		json_value_free(pJson);
		return false;
	}

	const json_value &Tuning = (*pJson)["tuning"];
	if(Tuning.type == json_object)
	{
		for(unsigned i = 0; i < Tuning.u.object.length; i++)
		{
			const char *pName = Tuning.u.object.values[i].name;
			const json_value &Value = *Tuning.u.object.values[i].value;
			if(Value.type != json_string)
			{
				continue;
			}
			const int RawValue = str_toint(json_string_get(&Value));
			for(int Index = 0; Index < CTuningParams::Num(); Index++)
			{
				if(str_comp_nocase(pName, CTuningParams::Name(Index)) == 0)
				{
					m_Header.m_Tuning.NetworkArray()[Index] = RawValue;
					break;
				}
			}
		}
	}

	json_value_free(pJson);

	m_pData = pBytes;
	m_Size = Size;
	m_Pos = (pNul - pBytes) + 1;
	return true;
}

size_t CTeeHistorianReader::UnconsumedBytesAfterFinish() const
{
	if(!m_Finished)
	{
		return 0;
	}
	return m_Size - m_Pos;
}

bool CTeeHistorianReader::SetCurrentTick(int64_t NewTick)
{
	if(NewTick < 0 || NewTick > MAX_PLAUSIBLE_TICK)
	{
		SetError("tick out of plausible range");
		return false;
	}
	m_CurrentTick = (int)NewTick;
	return true;
}

bool CTeeHistorianReader::AdvancePlayerDataTick(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
	{
		SetError("player data client id out of range");
		return false;
	}
	if(ClientId <= m_RunningMaxClientId)
	{
		// A non-ascending client id can only mean a new tick started
		// implicitly (see `CTeeHistorian::EnsureTickWrittenPlayerData`);
		// the writer would have emitted an explicit TICK_SKIP chunk
		// otherwise, which we'd have already consumed as its own opcode.
		if(!SetCurrentTick((int64_t)m_LastWrittenTick + 1))
		{
			return false;
		}
		m_RunningMaxClientId = -1;

		CTeeHistorianEvent TickEvent;
		TickEvent.m_Type = TEEHISTORIAN_READER_EVENT_TICK_SKIP;
		TickEvent.m_Tick = m_CurrentTick;
		TickEvent.m_TickIsExplicit = false;
		TickEvent.m_TickDelta = 0;
		m_QueuedEvents.push_back(TickEvent);
	}
	m_RunningMaxClientId = ClientId;
	m_LastWrittenTick = m_CurrentTick;
	return true;
}

// If `AdvancePlayerDataTick` queued an implicit TICK_SKIP event, that event
// takes priority: return it now and queue `DataEvent` to be returned by the
// next `NextEvent` call. Otherwise return `DataEvent` immediately.
void CTeeHistorianReader::QueueOrReturn(CTeeHistorianEvent *pOutEvent, const CTeeHistorianEvent &DataEvent)
{
	if(!m_QueuedEvents.empty())
	{
		*pOutEvent = m_QueuedEvents.front();
		m_QueuedEvents.erase(m_QueuedEvents.begin());
		m_QueuedEvents.push_back(DataEvent);
	}
	else
	{
		*pOutEvent = DataEvent;
	}
}

bool CTeeHistorianReader::HandleEx(CTeeHistorianEvent *pEvent)
{
	CUuid Uuid;
	if(!ReadUuid(&Uuid))
	{
		return false;
	}
	int DataSize;
	if(!ReadInt(&DataSize))
	{
		return false;
	}
	if(DataSize < 0 || (size_t)DataSize > m_Size - m_Pos)
	{
		SetError("invalid TEEHISTORIAN_EX data size");
		return false;
	}
	const size_t ChunkStart = m_Pos;
	const size_t ChunkEnd = ChunkStart + DataSize;

	pEvent->m_Tick = m_CurrentTick;

	int ChunkIndex = -1;
	for(size_t i = 0; i < NUM_TEEHISTORIAN_EX_CHUNKS; i++)
	{
		if(s_aExChunkUuids[i] == Uuid)
		{
			ChunkIndex = (int)i;
			break;
		}
	}

	bool Ok = true;
	if(ChunkIndex < 0)
	{
		pEvent->m_Type = TEEHISTORIAN_READER_EVENT_UNKNOWN_EX;
		pEvent->m_UnknownUuid = Uuid;
		const unsigned char *pRaw = ReadRaw((size_t)DataSize);
		if(!pRaw)
		{
			return false;
		}
		pEvent->m_vRawData.assign(pRaw, pRaw + DataSize);

		bool Found = false;
		for(auto &Entry : m_vUnknownExUuids)
		{
			if(Entry.first == Uuid)
			{
				Entry.second++;
				Found = true;
				break;
			}
		}
		if(!Found)
		{
			m_vUnknownExUuids.emplace_back(Uuid, 1);
		}
		m_NumUnknownExChunks++;
	}
	else
	{
		// No `default` label: adding a `UUID()` to teehistorian_ex_chunks.h without adding a
		// matching `case` here leaves this switch non-exhaustive, which is a `-Wswitch` error.
		switch((ETeeHistorianExChunk)ChunkIndex)
		{
		case EX_TEEHISTORIAN_TEST:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_TEST;
			break;

		case EX_TEEHISTORIAN_DDNETVER_OLD:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_DDNETVER_OLD;
			Ok = ReadClientId(&pEvent->m_ClientId) && ReadInt(&pEvent->m_DDNetVersion);
			break;

		case EX_TEEHISTORIAN_DDNETVER:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_DDNETVER;
			Ok = ReadClientId(&pEvent->m_ClientId) && ReadUuid(&pEvent->m_ConnectionId) &&
			     ReadInt(&pEvent->m_DDNetVersion) && ReadString(&pEvent->m_Str);
			break;

		case EX_TEEHISTORIAN_AUTH_INIT:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_AUTH_INIT;
			Ok = ReadClientId(&pEvent->m_ClientId) && ReadInt(&pEvent->m_AuthLevel) && ReadString(&pEvent->m_Str);
			break;

		case EX_TEEHISTORIAN_AUTH_LOGIN:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_AUTH_LOGIN;
			Ok = ReadClientId(&pEvent->m_ClientId) && ReadInt(&pEvent->m_AuthLevel) && ReadString(&pEvent->m_Str);
			break;

		case EX_TEEHISTORIAN_AUTH_LOGOUT:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_AUTH_LOGOUT;
			Ok = ReadClientId(&pEvent->m_ClientId);
			break;

		case EX_TEEHISTORIAN_JOINVER6:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_JOINVER6;
			Ok = ReadClientId(&pEvent->m_ClientId);
			break;

		case EX_TEEHISTORIAN_JOINVER7:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_JOINVER7;
			Ok = ReadClientId(&pEvent->m_ClientId);
			break;

		case EX_TEEHISTORIAN_PLAYER_SWITCH:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_PLAYER_SWITCH;
			Ok = ReadClientId(&pEvent->m_ClientId) && ReadClientId(&pEvent->m_ClientId2);
			break;

		case EX_TEEHISTORIAN_SAVE_SUCCESS:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_SAVE_SUCCESS;
			Ok = ReadTeamId(&pEvent->m_Team) && ReadUuid(&pEvent->m_SaveId) && ReadString(&pEvent->m_Str);
			break;

		case EX_TEEHISTORIAN_SAVE_FAILURE:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_SAVE_FAILURE;
			Ok = ReadTeamId(&pEvent->m_Team);
			break;

		case EX_TEEHISTORIAN_LOAD_SUCCESS:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_LOAD_SUCCESS;
			Ok = ReadTeamId(&pEvent->m_Team) && ReadUuid(&pEvent->m_SaveId) && ReadString(&pEvent->m_Str);
			break;

		case EX_TEEHISTORIAN_LOAD_FAILURE:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_LOAD_FAILURE;
			Ok = ReadTeamId(&pEvent->m_Team);
			break;

		case EX_TEEHISTORIAN_PLAYER_TEAM:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_PLAYER_TEAM;
			Ok = ReadClientId(&pEvent->m_ClientId) && ReadTeamId(&pEvent->m_Team);
			break;

		case EX_TEEHISTORIAN_TEAM_PRACTICE:
		{
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_TEAM_PRACTICE;
			int Practice = 0;
			Ok = ReadTeamId(&pEvent->m_Team) && ReadInt(&Practice);
			pEvent->m_Practice = Ok && Practice != 0;
			break;
		}

		case EX_TEEHISTORIAN_PLAYER_READY:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_PLAYER_READY;
			Ok = ReadClientId(&pEvent->m_ClientId);
			break;

		case EX_TEEHISTORIAN_PLAYER_REJOIN:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_PLAYER_REJOIN;
			Ok = ReadClientId(&pEvent->m_ClientId);
			break;

		case EX_TEEHISTORIAN_ANTIBOT:
		{
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_ANTIBOT;
			const unsigned char *pRaw = ReadRaw((size_t)DataSize);
			Ok = pRaw != nullptr;
			if(Ok)
			{
				pEvent->m_vRawData.assign(pRaw, pRaw + DataSize);
			}
			break;
		}

		case EX_TEEHISTORIAN_PLAYER_NAME:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_PLAYER_NAME;
			Ok = ReadClientId(&pEvent->m_ClientId) && ReadString(&pEvent->m_Str);
			break;

		case EX_TEEHISTORIAN_PLAYER_FINISH:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_PLAYER_FINISH;
			Ok = ReadClientId(&pEvent->m_ClientId) && ReadInt(&pEvent->m_TimeTicks);
			break;

		case EX_TEEHISTORIAN_TEAM_FINISH:
			pEvent->m_Type = TEEHISTORIAN_READER_EVENT_TEAM_FINISH;
			Ok = ReadTeamId(&pEvent->m_Team) && ReadInt(&pEvent->m_TimeTicks);
			break;
		}
	}

	if(!Ok || m_Error)
	{
		return false;
	}
	// Chunks are length-prefixed: forward-compatibly resync to the declared
	// end regardless of how many bytes the specific parser above consumed.
	if(m_Pos > ChunkEnd)
	{
		SetError("TEEHISTORIAN_EX chunk payload overran its declared size");
		return false;
	}
	m_Pos = ChunkEnd;
	return true;
}

bool CTeeHistorianReader::NextEvent(CTeeHistorianEvent *pEvent)
{
	if(!m_QueuedEvents.empty())
	{
		*pEvent = m_QueuedEvents.front();
		m_QueuedEvents.erase(m_QueuedEvents.begin());
		return true;
	}
	if(m_Error || m_Finished)
	{
		return false;
	}
	if(AtEnd())
	{
		// Clean end of the buffer between events, not a malformed read: the
		// recording just doesn't (yet, or ever) have a FINISH chunk.
		return false;
	}

	*pEvent = CTeeHistorianEvent();

	int Opcode;
	if(!ReadInt(&Opcode))
	{
		return false;
	}

	if(Opcode >= 0)
	{
		// Bare non-negative int: a client id introducing a position delta
		// against the last known (alive) position of that player.
		const int ClientId = Opcode;
		if(!AdvancePlayerDataTick(ClientId))
		{
			return false;
		}
		int Dx, Dy;
		if(!ReadInt(&Dx) || !ReadInt(&Dy))
		{
			return false;
		}
		CPlayerState &Player = m_aPlayers[ClientId];
		if(!Player.m_Alive)
		{
			SetError("position delta for a player that isn't alive");
			return false;
		}
		// Unsigned arithmetic deliberately: an attacker-controlled `Dx`/`Dy` must wrap like a
		// normal two's-complement add instead of triggering signed-overflow UB (see the input
		// diff reconstruction below, which does the same).
		Player.m_X = (int)((unsigned)Player.m_X + (unsigned)Dx);
		Player.m_Y = (int)((unsigned)Player.m_Y + (unsigned)Dy);

		CTeeHistorianEvent DiffEvent;
		DiffEvent.m_Type = TEEHISTORIAN_READER_EVENT_PLAYER_DIFF;
		DiffEvent.m_Tick = m_CurrentTick;
		DiffEvent.m_ClientId = ClientId;
		DiffEvent.m_X = Player.m_X;
		DiffEvent.m_Y = Player.m_Y;
		QueueOrReturn(pEvent, DiffEvent);
		return true;
	}

	const int Type = -Opcode;
	switch(Type)
	{
	case TEEHISTORIAN_FINISH:
		pEvent->m_Type = TEEHISTORIAN_READER_EVENT_FINISH;
		pEvent->m_Tick = m_CurrentTick;
		m_Finished = true;
		return true;

	case TEEHISTORIAN_TICK_SKIP:
	{
		int Delta;
		if(!ReadInt(&Delta))
		{
			return false;
		}
		if(Delta < 0)
		{
			SetError("negative tick skip delta");
			return false;
		}
		if(!SetCurrentTick((int64_t)m_LastWrittenTick + (int64_t)Delta + 1))
		{
			return false;
		}
		m_LastWrittenTick = m_CurrentTick;
		m_RunningMaxClientId = -1;

		pEvent->m_Type = TEEHISTORIAN_READER_EVENT_TICK_SKIP;
		pEvent->m_Tick = m_CurrentTick;
		pEvent->m_TickIsExplicit = true;
		pEvent->m_TickDelta = Delta;
		return true;
	}

	case TEEHISTORIAN_PLAYER_NEW:
	{
		int ClientId, X, Y;
		if(!ReadInt(&ClientId))
		{
			return false;
		}
		if(!AdvancePlayerDataTick(ClientId))
		{
			return false;
		}
		if(!ReadInt(&X) || !ReadInt(&Y))
		{
			return false;
		}
		m_aPlayers[ClientId] = {true, X, Y};

		CTeeHistorianEvent NewEvent;
		NewEvent.m_Type = TEEHISTORIAN_READER_EVENT_PLAYER_NEW;
		NewEvent.m_Tick = m_CurrentTick;
		NewEvent.m_ClientId = ClientId;
		NewEvent.m_X = X;
		NewEvent.m_Y = Y;
		QueueOrReturn(pEvent, NewEvent);
		return true;
	}

	case TEEHISTORIAN_PLAYER_OLD:
	{
		int ClientId;
		if(!ReadInt(&ClientId))
		{
			return false;
		}
		if(!AdvancePlayerDataTick(ClientId))
		{
			return false;
		}
		m_aPlayers[ClientId].m_Alive = false;

		CTeeHistorianEvent OldEvent;
		OldEvent.m_Type = TEEHISTORIAN_READER_EVENT_PLAYER_OLD;
		OldEvent.m_Tick = m_CurrentTick;
		OldEvent.m_ClientId = ClientId;
		QueueOrReturn(pEvent, OldEvent);
		return true;
	}

	case TEEHISTORIAN_INPUT_DIFF:
	case TEEHISTORIAN_INPUT_NEW:
	{
		int ClientId;
		if(!ReadClientId(&ClientId))
		{
			return false;
		}
		int aValues[sizeof(CNetObj_PlayerInput) / sizeof(int32_t)];
		for(int &Value : aValues)
		{
			if(!ReadInt(&Value))
			{
				return false;
			}
		}
		CInputState &Input = m_aInputs[ClientId];
		int *pFields = (int *)&pEvent->m_Input;
		if(Type == TEEHISTORIAN_INPUT_NEW)
		{
			mem_copy(pFields, aValues, sizeof(aValues));
		}
		else
		{
			if(!Input.m_Have)
			{
				SetError("input diff for a player without a previous input");
				return false;
			}
			const int *pPrev = (const int *)&Input.m_Input;
			for(size_t i = 0; i < std::size(aValues); i++)
			{
				pFields[i] = (int)((unsigned)pPrev[i] + (unsigned)aValues[i]);
			}
		}
		Input.m_Have = true;
		Input.m_Input = pEvent->m_Input;

		pEvent->m_Type = Type == TEEHISTORIAN_INPUT_NEW ? TEEHISTORIAN_READER_EVENT_INPUT_NEW : TEEHISTORIAN_READER_EVENT_INPUT_DIFF;
		pEvent->m_Tick = m_CurrentTick;
		pEvent->m_ClientId = ClientId;
		return true;
	}

	case TEEHISTORIAN_MESSAGE:
	{
		int ClientId, MsgSize;
		if(!ReadClientId(&ClientId) || !ReadInt(&MsgSize))
		{
			return false;
		}
		if(MsgSize < 0)
		{
			SetError("negative message size");
			return false;
		}
		const unsigned char *pRaw = ReadRaw((size_t)MsgSize);
		if(!pRaw)
		{
			return false;
		}
		pEvent->m_Type = TEEHISTORIAN_READER_EVENT_MESSAGE;
		pEvent->m_Tick = m_CurrentTick;
		pEvent->m_ClientId = ClientId;
		pEvent->m_vRawData.assign(pRaw, pRaw + MsgSize);
		return true;
	}

	case TEEHISTORIAN_JOIN:
	{
		int ClientId;
		if(!ReadClientId(&ClientId))
		{
			return false;
		}
		pEvent->m_Type = TEEHISTORIAN_READER_EVENT_JOIN;
		pEvent->m_Tick = m_CurrentTick;
		pEvent->m_ClientId = ClientId;
		return true;
	}

	case TEEHISTORIAN_DROP:
	{
		int ClientId;
		if(!ReadClientId(&ClientId) || !ReadString(&pEvent->m_Str))
		{
			return false;
		}
		pEvent->m_Type = TEEHISTORIAN_READER_EVENT_DROP;
		pEvent->m_Tick = m_CurrentTick;
		pEvent->m_ClientId = ClientId;
		return true;
	}

	case TEEHISTORIAN_CONSOLE_COMMAND:
	{
		int ClientId, FlagMask, NumArgs;
		if(!ReadClientIdOrUnspecified(&ClientId) || !ReadInt(&FlagMask) || !ReadString(&pEvent->m_Str) || !ReadInt(&NumArgs))
		{
			return false;
		}
		if(NumArgs < 0)
		{
			SetError("negative console command argument count");
			return false;
		}
		for(int i = 0; i < NumArgs; i++)
		{
			std::string Arg;
			if(!ReadString(&Arg))
			{
				return false;
			}
			pEvent->m_vArgs.push_back(std::move(Arg));
		}
		pEvent->m_Type = TEEHISTORIAN_READER_EVENT_CONSOLE_COMMAND;
		pEvent->m_Tick = m_CurrentTick;
		pEvent->m_ClientId = ClientId;
		pEvent->m_FlagMask = FlagMask;
		return true;
	}

	case TEEHISTORIAN_EX:
		return HandleEx(pEvent);

	default:
		SetError("unknown top-level message type (not forward-compatibly skippable)");
		return false;
	}
}
