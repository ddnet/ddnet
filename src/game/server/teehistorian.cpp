#include "teehistorian.h"

#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/shared/packer.h>
#include <engine/shared/snapshot.h>

#include <game/gamecore.h>

class CTeehistorianPacker : public CAbstractPacker
{
public:
	CTeehistorianPacker() :
		CAbstractPacker(m_aBuffer, sizeof(m_aBuffer))
	{
	}

private:
	unsigned char m_aBuffer[1024 * 64];
};

static const char TEEHISTORIAN_NAME[] = "teehistorian@ddnet.tw";
static const CUuid TEEHISTORIAN_UUID = CalculateUuid(TEEHISTORIAN_NAME);
static const char TEEHISTORIAN_VERSION[] = "2";
static const char TEEHISTORIAN_VERSION_MINOR[] = "9";

#define UUID(id, name) static const CUuid UUID_##id = CalculateUuid(name);
#include <engine/shared/teehistorian_ex_chunks.h>
#undef UUID

enum
{
	TEEHISTORIAN_NONE,
	TEEHISTORIAN_FINISH,
	TEEHISTORIAN_TICK_SKIP,
	TEEHISTORIAN_PLAYER_NEW,
	TEEHISTORIAN_PLAYER_OLD,
	TEEHISTORIAN_INPUT_DIFF,
	TEEHISTORIAN_INPUT_NEW,
	TEEHISTORIAN_MESSAGE,
	TEEHISTORIAN_JOIN,
	TEEHISTORIAN_DROP,
	TEEHISTORIAN_CONSOLE_COMMAND,
	TEEHISTORIAN_EX,
};

CTeeHistorian::CTeeHistorian()
{
	m_State = STATE_START;
	m_pfnWriteCallback = nullptr;
	m_pWriteCallbackUserdata = nullptr;
}

void CTeeHistorian::Reset(const CGameInfo *pGameInfo, WRITE_CALLBACK pfnWriteCallback, void *pUser)
{
	dbg_assert(m_State == STATE_START || m_State == STATE_BEFORE_TICK, "invalid teehistorian state");

	m_Debug = 0;

	m_Tick = 0;
	m_LastWrittenTick = 0;
	// Tick 0 is implicit at the start, game starts as tick 1.
	m_TickWritten = true;
	m_MaxClientId = MAX_CLIENTS;

	// `m_PrevMaxClientId` is initialized in `BeginPlayers`
	for(auto &PrevPlayer : m_aPrevPlayers)
	{
		PrevPlayer.m_Alive = false;
		// zero means no id
		PrevPlayer.m_UniqueClientId = 0;
		PrevPlayer.m_Team = 0;
	}
	for(auto &PrevTeam : m_aPrevTeams)
	{
		PrevTeam.m_Practice = false;
	}
	m_pfnWriteCallback = pfnWriteCallback;
	m_pWriteCallbackUserdata = pUser;

	WriteHeader(pGameInfo);

	m_State = STATE_START;
}

void CTeeHistorian::WriteHeader(const CGameInfo *pGameInfo)
{
	Write(&TEEHISTORIAN_UUID, sizeof(TEEHISTORIAN_UUID));

	char aGameUuid[UUID_MAXSTRSIZE];
	char aStartTime[128];
	char aMapSha256[SHA256_MAXSTRSIZE];

	FormatUuid(pGameInfo->m_GameUuid, aGameUuid, sizeof(aGameUuid));
	str_timestamp_ex(pGameInfo->m_StartTime, aStartTime, sizeof(aStartTime), "%Y-%m-%dT%H:%M:%S%z");
	sha256_str(pGameInfo->m_MapSha256, aMapSha256, sizeof(aMapSha256));

	char aPrevGameUuid[UUID_MAXSTRSIZE];
	char aPrevGameUuidJson[64];
	if(pGameInfo->m_HavePrevGameUuid)
	{
		FormatUuid(pGameInfo->m_PrevGameUuid, aPrevGameUuid, sizeof(aPrevGameUuid));
		str_format(aPrevGameUuidJson, sizeof(aPrevGameUuidJson), "\"prev_game_uuid\":\"%s\",", aPrevGameUuid);
	}
	else
	{
		aPrevGameUuidJson[0] = 0;
	}

	char aCommentBuffer[128];
	char aServerVersionBuffer[128];
	char aStartTimeBuffer[128];
	char aServerNameBuffer[128];
	char aGameTypeBuffer[128];
	char aMapNameBuffer[128];
	char aMapSha256Buffer[256];
	char aPrngDescription[128];

	char aJson[2048];

#define E(buf, str) EscapeJson(buf, sizeof(buf), str)

	str_format(aJson, sizeof(aJson),
		"{"
		"\"comment\":\"%s\","
		"\"version\":\"%s\","
		"\"version_minor\":\"%s\","
		"\"game_uuid\":\"%s\","
		"%s"
		"\"server_version\":\"%s\","
		"\"start_time\":\"%s\","
		"\"server_name\":\"%s\","
		"\"server_port\":\"%d\","
		"\"game_type\":\"%s\","
		"\"map_name\":\"%s\","
		"\"map_size\":\"%d\","
		"\"map_sha256\":\"%s\","
		"\"map_crc\":\"%08x\","
		"\"prng_description\":\"%s\","
		"\"config\":{",
		E(aCommentBuffer, TEEHISTORIAN_NAME),
		TEEHISTORIAN_VERSION,
		TEEHISTORIAN_VERSION_MINOR,
		aGameUuid,
		aPrevGameUuidJson,
		E(aServerVersionBuffer, pGameInfo->m_pServerVersion),
		E(aStartTimeBuffer, aStartTime),
		E(aServerNameBuffer, pGameInfo->m_pServerName),
		pGameInfo->m_ServerPort,
		E(aGameTypeBuffer, pGameInfo->m_pGameType),
		E(aMapNameBuffer, pGameInfo->m_pMapName),
		pGameInfo->m_MapSize,
		E(aMapSha256Buffer, aMapSha256),
		pGameInfo->m_MapCrc,
		E(aPrngDescription, pGameInfo->m_pPrngDescription));
	Write(aJson, str_length(aJson));

	char aBuffer1[1024];
	char aBuffer2[1024];
	bool First = true;

#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	if((Flags)&CFGFLAG_SERVER && !((Flags)&CFGFLAG_NONTEEHISTORIC) && pGameInfo->m_pConfig->m_##Name != (Def)) \
	{ \
		str_format(aJson, sizeof(aJson), "%s\"%s\":\"%d\"", \
			First ? "" : ",", \
			E(aBuffer1, #ScriptName), \
			pGameInfo->m_pConfig->m_##Name); \
		Write(aJson, str_length(aJson)); \
		First = false; \
	}

#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) MACRO_CONFIG_INT(Name, ScriptName, Def, 0, 0, Flags, Desc)

#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	if((Flags)&CFGFLAG_SERVER && !((Flags)&CFGFLAG_NONTEEHISTORIC) && str_comp(pGameInfo->m_pConfig->m_##Name, (Def)) != 0) \
	{ \
		str_format(aJson, sizeof(aJson), "%s\"%s\":\"%s\"", \
			First ? "" : ",", \
			E(aBuffer1, #ScriptName), \
			E(aBuffer2, pGameInfo->m_pConfig->m_##Name)); \
		Write(aJson, str_length(aJson)); \
		First = false; \
	}

#include <engine/shared/config_variables.h>

#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR

	str_copy(aJson, "},\"tuning\":{");
	Write(aJson, str_length(aJson));

	First = true;

#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) \
	if(pGameInfo->m_pTuning->m_##Name.Get() != (int)((Value)*100)) \
	{ \
		str_format(aJson, sizeof(aJson), "%s\"%s\":\"%d\"", \
			First ? "" : ",", \
			E(aBuffer1, #ScriptName), \
			pGameInfo->m_pTuning->m_##Name.Get()); \
		Write(aJson, str_length(aJson)); \
		First = false; \
	}
#include <game/tuning.h>
#undef MACRO_TUNING_PARAM

	str_copy(aJson, "},\"uuids\":[");
	Write(aJson, str_length(aJson));

	for(int i = 0; i < pGameInfo->m_pUuids->NumUuids(); i++)
	{
		str_format(aJson, sizeof(aJson), "%s\"%s\"",
			i == 0 ? "" : ",",
			E(aBuffer1, pGameInfo->m_pUuids->GetName(OFFSET_UUID + i)));
		Write(aJson, str_length(aJson));
	}

	str_copy(aJson, "]}");
	Write(aJson, str_length(aJson));
	Write("", 1); // Null termination.
}

void CTeeHistorian::WriteExtra(CUuid Uuid, const void *pData, int DataSize)
{
	EnsureTickWritten();

	CTeehistorianPacker Ex;
	Ex.Reset();
	Ex.AddInt(-TEEHISTORIAN_EX);
	Ex.AddRaw(&Uuid, sizeof(Uuid));
	Ex.AddInt(DataSize);
	Write(Ex.Data(), Ex.Size());
	Write(pData, DataSize);
}

void CTeeHistorian::BeginTick(int Tick)
{
	dbg_assert(m_State == STATE_START || m_State == STATE_BEFORE_TICK, "invalid teehistorian state");

	m_Tick = Tick;
	m_TickWritten = false;

	if(m_Debug > 1)
	{
		dbg_msg("teehistorian", "tick %d", Tick);
	}

	m_State = STATE_BEFORE_PLAYERS;
}

void CTeeHistorian::BeginPlayers()
{
	dbg_assert(m_State == STATE_BEFORE_PLAYERS, "invalid teehistorian state");

	m_PrevMaxClientId = m_MaxClientId;
	// ensure that PLAYER_{DIFF, NEW, OLD} don't cause an implicit tick after a TICK_SKIP
	// by not overwriting m_MaxClientId during RecordPlayer
	m_MaxClientId = -1;

	m_State = STATE_PLAYERS;
}

void CTeeHistorian::EnsureTickWrittenPlayerData(int ClientId)
{
	dbg_assert(ClientId > m_MaxClientId, "invalid player data order");
	m_MaxClientId = ClientId;

	if(!m_TickWritten && (ClientId > m_PrevMaxClientId || m_LastWrittenTick + 1 != m_Tick))
	{
		WriteTick();
	}
	else
	{
		// Tick is implicit.
		m_LastWrittenTick = m_Tick;
		m_TickWritten = true;
	}
}

void CTeeHistorian::RecordPlayer(int ClientId, const CNetObj_CharacterCore *pChar)
{
	dbg_assert(m_State == STATE_PLAYERS, "invalid teehistorian state");

	CTeehistorianPlayer *pPrev = &m_aPrevPlayers[ClientId];
	if(!pPrev->m_Alive || pPrev->m_X != pChar->m_X || pPrev->m_Y != pChar->m_Y)
	{
		EnsureTickWrittenPlayerData(ClientId);

		CTeehistorianPacker Buffer;
		Buffer.Reset();
		if(pPrev->m_Alive)
		{
			int dx = pChar->m_X - pPrev->m_X;
			int dy = pChar->m_Y - pPrev->m_Y;
			Buffer.AddInt(ClientId);
			Buffer.AddInt(dx);
			Buffer.AddInt(dy);
			if(m_Debug)
			{
				dbg_msg("teehistorian", "diff cid=%d dx=%d dy=%d", ClientId, dx, dy);
			}
		}
		else
		{
			int x = pChar->m_X;
			int y = pChar->m_Y;
			Buffer.AddInt(-TEEHISTORIAN_PLAYER_NEW);
			Buffer.AddInt(ClientId);
			Buffer.AddInt(x);
			Buffer.AddInt(y);
			if(m_Debug)
			{
				dbg_msg("teehistorian", "new cid=%d x=%d y=%d", ClientId, x, y);
			}
		}
		Write(Buffer.Data(), Buffer.Size());
	}
	pPrev->m_X = pChar->m_X;
	pPrev->m_Y = pChar->m_Y;
	pPrev->m_Alive = true;
}

void CTeeHistorian::RecordDeadPlayer(int ClientId)
{
	dbg_assert(m_State == STATE_PLAYERS, "invalid teehistorian state");

	CTeehistorianPlayer *pPrev = &m_aPrevPlayers[ClientId];
	if(pPrev->m_Alive)
	{
		EnsureTickWrittenPlayerData(ClientId);

		CTeehistorianPacker Buffer;
		Buffer.Reset();
		Buffer.AddInt(-TEEHISTORIAN_PLAYER_OLD);
		Buffer.AddInt(ClientId);
		if(m_Debug)
		{
			dbg_msg("teehistorian", "old cid=%d", ClientId);
		}
		Write(Buffer.Data(), Buffer.Size());
	}
	pPrev->m_Alive = false;
}

void CTeeHistorian::RecordPlayerTeam(int ClientId, int Team)
{
	if(m_aPrevPlayers[ClientId].m_Team != Team)
	{
		m_aPrevPlayers[ClientId].m_Team = Team;

		EnsureTickWritten();

		CTeehistorianPacker Buffer;
		Buffer.Reset();
		Buffer.AddInt(ClientId);
		Buffer.AddInt(Team);

		if(m_Debug)
		{
			dbg_msg("teehistorian", "player_team cid=%d team=%d", ClientId, Team);
		}

		WriteExtra(UUID_TEEHISTORIAN_PLAYER_TEAM, Buffer.Data(), Buffer.Size());
	}
}

void CTeeHistorian::RecordTeamPractice(int Team, bool Practice)
{
	if(m_aPrevTeams[Team].m_Practice != Practice)
	{
		m_aPrevTeams[Team].m_Practice = Practice;

		EnsureTickWritten();

		CTeehistorianPacker Buffer;
		Buffer.Reset();
		Buffer.AddInt(Team);
		Buffer.AddInt(Practice);

		if(m_Debug)
		{
			dbg_msg("teehistorian", "team_practice team=%d practice=%d", Team, Practice);
		}

		WriteExtra(UUID_TEEHISTORIAN_TEAM_PRACTICE, Buffer.Data(), Buffer.Size());
	}
}

void CTeeHistorian::Write(const void *pData, int DataSize)
{
	m_pfnWriteCallback(pData, DataSize, m_pWriteCallbackUserdata);
}

void CTeeHistorian::EnsureTickWritten()
{
	if(!m_TickWritten)
	{
		WriteTick();
	}
}

void CTeeHistorian::WriteTick()
{
	CTeehistorianPacker TickPacker;
	TickPacker.Reset();

	int dt = m_Tick - m_LastWrittenTick - 1;
	TickPacker.AddInt(-TEEHISTORIAN_TICK_SKIP);
	TickPacker.AddInt(dt);
	if(m_Debug)
	{
		dbg_msg("teehistorian", "skip_ticks dt=%d", dt);
	}
	Write(TickPacker.Data(), TickPacker.Size());

	m_TickWritten = true;
	m_LastWrittenTick = m_Tick;
}

void CTeeHistorian::EndPlayers()
{
	dbg_assert(m_State == STATE_PLAYERS, "invalid teehistorian state");

	m_State = STATE_BEFORE_INPUTS;
}

void CTeeHistorian::BeginInputs()
{
	dbg_assert(m_State == STATE_BEFORE_INPUTS, "invalid teehistorian state");

	m_State = STATE_INPUTS;
}

void CTeeHistorian::RecordPlayerInput(int ClientId, uint32_t UniqueClientId, const CNetObj_PlayerInput *pInput)
{
	CTeehistorianPacker Buffer;

	CTeehistorianPlayer *pPrev = &m_aPrevPlayers[ClientId];
	CNetObj_PlayerInput DiffInput;
	if(pPrev->m_UniqueClientId == UniqueClientId)
	{
		if(mem_comp(&pPrev->m_Input, pInput, sizeof(pPrev->m_Input)) == 0)
		{
			return;
		}
		EnsureTickWritten();
		Buffer.Reset();

		Buffer.AddInt(-TEEHISTORIAN_INPUT_DIFF);
		CSnapshotDelta::DiffItem((int *)&pPrev->m_Input, (int *)pInput, (int *)&DiffInput, sizeof(DiffInput) / sizeof(int32_t));
		if(m_Debug)
		{
			const int *pData = (const int *)&DiffInput;
			dbg_msg("teehistorian", "diff_input cid=%d %d %d %d %d %d %d %d %d %d %d", ClientId,
				pData[0], pData[1], pData[2], pData[3], pData[4],
				pData[5], pData[6], pData[7], pData[8], pData[9]);
		}
	}
	else
	{
		EnsureTickWritten();
		Buffer.Reset();
		Buffer.AddInt(-TEEHISTORIAN_INPUT_NEW);
		DiffInput = *pInput;
		if(m_Debug)
		{
			dbg_msg("teehistorian", "new_input cid=%d", ClientId);
		}
	}
	Buffer.AddInt(ClientId);
	for(size_t i = 0; i < sizeof(DiffInput) / sizeof(int32_t); i++)
	{
		Buffer.AddInt(((int *)&DiffInput)[i]);
	}
	pPrev->m_UniqueClientId = UniqueClientId;
	pPrev->m_Input = *pInput;

	Write(Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordPlayerMessage(int ClientId, const void *pMsg, int MsgSize)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(-TEEHISTORIAN_MESSAGE);
	Buffer.AddInt(ClientId);
	Buffer.AddInt(MsgSize);
	Buffer.AddRaw(pMsg, MsgSize);

	if(m_Debug)
	{
		CUnpacker Unpacker;
		Unpacker.Reset(pMsg, MsgSize);
		int MsgId = Unpacker.GetInt();
		int Sys = MsgId & 1;
		MsgId >>= 1;
		dbg_msg("teehistorian", "msg cid=%d sys=%d msgid=%d", ClientId, Sys, MsgId);
	}

	Write(Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordPlayerJoin(int ClientId, int Protocol)
{
	dbg_assert(Protocol == PROTOCOL_6 || Protocol == PROTOCOL_7, "invalid version");
	EnsureTickWritten();

	{
		CTeehistorianPacker Buffer;
		Buffer.Reset();
		Buffer.AddInt(ClientId);
		if(m_Debug)
		{
			dbg_msg("teehistorian", "joinver%d cid=%d", Protocol == PROTOCOL_6 ? 6 : 7, ClientId);
		}
		CUuid Uuid = Protocol == PROTOCOL_6 ? UUID_TEEHISTORIAN_JOINVER6 : UUID_TEEHISTORIAN_JOINVER7;
		WriteExtra(Uuid, Buffer.Data(), Buffer.Size());
	}

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(-TEEHISTORIAN_JOIN);
	Buffer.AddInt(ClientId);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "join cid=%d", ClientId);
	}

	Write(Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordPlayerRejoin(int ClientId)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "player_rejoin cid=%d", ClientId);
	}

	WriteExtra(UUID_TEEHISTORIAN_PLAYER_REJOIN, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordPlayerReady(int ClientId)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "player_ready cid=%d", ClientId);
	}

	WriteExtra(UUID_TEEHISTORIAN_PLAYER_READY, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordPlayerDrop(int ClientId, const char *pReason)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(-TEEHISTORIAN_DROP);
	Buffer.AddInt(ClientId);
	Buffer.AddString(pReason, 0);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "drop cid=%d reason='%s'", ClientId, pReason);
	}

	Write(Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordPlayerName(int ClientId, const char *pName)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId);
	Buffer.AddString(pName, 0);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "player_name cid=%d name='%s'", ClientId, pName);
	}

	WriteExtra(UUID_TEEHISTORIAN_PLAYER_NAME, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordConsoleCommand(int ClientId, int FlagMask, const char *pCmd, IConsole::IResult *pResult)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(-TEEHISTORIAN_CONSOLE_COMMAND);
	Buffer.AddInt(ClientId);
	Buffer.AddInt(FlagMask);
	Buffer.AddString(pCmd, 0);
	Buffer.AddInt(pResult->NumArguments());
	for(int i = 0; i < pResult->NumArguments(); i++)
	{
		Buffer.AddString(pResult->GetString(i), 0);
	}

	if(m_Debug)
	{
		dbg_msg("teehistorian", "ccmd cid=%d cmd='%s'", ClientId, pCmd);
	}

	Write(Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordTestExtra()
{
	if(m_Debug)
	{
		dbg_msg("teehistorian", "test");
	}

	WriteExtra(UUID_TEEHISTORIAN_TEST, "", 0);
}

void CTeeHistorian::RecordPlayerSwap(int ClientId1, int ClientId2)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId1);
	Buffer.AddInt(ClientId2);

	WriteExtra(UUID_TEEHISTORIAN_PLAYER_SWITCH, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordTeamSaveSuccess(int Team, CUuid SaveId, const char *pTeamSave)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(Team);
	Buffer.AddRaw(&SaveId, sizeof(SaveId));
	Buffer.AddString(pTeamSave, 0);

	if(m_Debug)
	{
		char aSaveId[UUID_MAXSTRSIZE];
		FormatUuid(SaveId, aSaveId, sizeof(aSaveId));
		dbg_msg("teehistorian", "save_success team=%d save_id=%s team_save='%s'", Team, aSaveId, pTeamSave);
	}

	WriteExtra(UUID_TEEHISTORIAN_SAVE_SUCCESS, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordTeamSaveFailure(int Team)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(Team);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "save_failure team=%d", Team);
	}

	WriteExtra(UUID_TEEHISTORIAN_SAVE_FAILURE, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordTeamLoadSuccess(int Team, CUuid SaveId, const char *pTeamSave)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(Team);
	Buffer.AddRaw(&SaveId, sizeof(SaveId));
	Buffer.AddString(pTeamSave, 0);

	if(m_Debug)
	{
		char aSaveId[UUID_MAXSTRSIZE];
		FormatUuid(SaveId, aSaveId, sizeof(aSaveId));
		dbg_msg("teehistorian", "load_success team=%d save_id=%s team_save='%s'", Team, aSaveId, pTeamSave);
	}

	WriteExtra(UUID_TEEHISTORIAN_LOAD_SUCCESS, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordTeamLoadFailure(int Team)
{
	EnsureTickWritten();

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(Team);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "load_failure team=%d", Team);
	}

	WriteExtra(UUID_TEEHISTORIAN_LOAD_FAILURE, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::EndInputs()
{
	dbg_assert(m_State == STATE_INPUTS, "invalid teehistorian state");

	m_State = STATE_BEFORE_ENDTICK;
}

void CTeeHistorian::EndTick()
{
	dbg_assert(m_State == STATE_BEFORE_ENDTICK, "invalid teehistorian state");
	m_State = STATE_BEFORE_TICK;
}

void CTeeHistorian::RecordDDNetVersionOld(int ClientId, int DDNetVersion)
{
	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId);
	Buffer.AddInt(DDNetVersion);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "ddnetver_old cid=%d ddnet_version=%d", ClientId, DDNetVersion);
	}

	WriteExtra(UUID_TEEHISTORIAN_DDNETVER_OLD, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordDDNetVersion(int ClientId, CUuid ConnectionId, int DDNetVersion, const char *pDDNetVersionStr)
{
	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId);
	Buffer.AddRaw(&ConnectionId, sizeof(ConnectionId));
	Buffer.AddInt(DDNetVersion);
	Buffer.AddString(pDDNetVersionStr, 0);

	if(m_Debug)
	{
		char aConnnectionId[UUID_MAXSTRSIZE];
		FormatUuid(ConnectionId, aConnnectionId, sizeof(aConnnectionId));
		dbg_msg("teehistorian", "ddnetver cid=%d connection_id=%s ddnet_version=%d ddnet_version_str=%s", ClientId, aConnnectionId, DDNetVersion, pDDNetVersionStr);
	}

	WriteExtra(UUID_TEEHISTORIAN_DDNETVER, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordAuthInitial(int ClientId, int Level, const char *pAuthName)
{
	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId);
	Buffer.AddInt(Level);
	Buffer.AddString(pAuthName, 0);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "auth_init cid=%d level=%d auth_name=%s", ClientId, Level, pAuthName);
	}

	WriteExtra(UUID_TEEHISTORIAN_AUTH_INIT, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordAuthLogin(int ClientId, int Level, const char *pAuthName)
{
	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId);
	Buffer.AddInt(Level);
	Buffer.AddString(pAuthName, 0);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "auth_login cid=%d level=%d auth_name=%s", ClientId, Level, pAuthName);
	}

	WriteExtra(UUID_TEEHISTORIAN_AUTH_LOGIN, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordAuthLogout(int ClientId)
{
	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "auth_logout cid=%d", ClientId);
	}

	WriteExtra(UUID_TEEHISTORIAN_AUTH_LOGOUT, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordAntibot(const void *pData, int DataSize)
{
	if(m_Debug)
	{
		dbg_msg("teehistorian", "antibot data_size=%d", DataSize);
	}

	WriteExtra(UUID_TEEHISTORIAN_ANTIBOT, pData, DataSize);
}

void CTeeHistorian::RecordPlayerFinish(int ClientId, int TimeTicks)
{
	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(ClientId);
	Buffer.AddInt(TimeTicks);
	if(m_Debug)
	{
		dbg_msg("teehistorian", "player_finish cid=%d time=%d", ClientId, TimeTicks);
	}

	WriteExtra(UUID_TEEHISTORIAN_PLAYER_FINISH, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::RecordTeamFinish(int TeamId, int TimeTicks)
{
	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(TeamId);
	Buffer.AddInt(TimeTicks);
	if(m_Debug)
	{
		dbg_msg("teehistorian", "team_finish cid=%d time=%d", TeamId, TimeTicks);
	}

	WriteExtra(UUID_TEEHISTORIAN_TEAM_FINISH, Buffer.Data(), Buffer.Size());
}

void CTeeHistorian::Finish()
{
	dbg_assert(m_State == STATE_START || m_State == STATE_INPUTS || m_State == STATE_BEFORE_ENDTICK || m_State == STATE_BEFORE_TICK, "invalid teehistorian state");

	if(m_State == STATE_INPUTS)
	{
		EndInputs();
	}
	if(m_State == STATE_BEFORE_ENDTICK)
	{
		EndTick();
	}

	CTeehistorianPacker Buffer;
	Buffer.Reset();
	Buffer.AddInt(-TEEHISTORIAN_FINISH);

	if(m_Debug)
	{
		dbg_msg("teehistorian", "finish");
	}

	Write(Buffer.Data(), Buffer.Size());
}
