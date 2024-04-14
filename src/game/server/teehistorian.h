#ifndef GAME_SERVER_TEEHISTORIAN_H
#define GAME_SERVER_TEEHISTORIAN_H

#include <base/hash.h>
#include <engine/console.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>

#include <ctime>

class CConfig;
class CTuningParams;
class CUuidManager;

class CTeeHistorian
{
public:
	typedef void (*WRITE_CALLBACK)(const void *pData, int DataSize, void *pUser);

	struct CGameInfo
	{
		CUuid m_GameUuid;
		const char *m_pServerVersion;
		time_t m_StartTime;
		const char *m_pPrngDescription;

		const char *m_pServerName;
		int m_ServerPort;
		const char *m_pGameType;

		const char *m_pMapName;
		int m_MapSize;
		SHA256_DIGEST m_MapSha256;
		int m_MapCrc;

		bool m_HavePrevGameUuid;
		CUuid m_PrevGameUuid;

		CConfig *m_pConfig;
		CTuningParams *m_pTuning;
		CUuidManager *m_pUuids;
	};

	enum
	{
		PROTOCOL_6 = 1,
		PROTOCOL_7,
	};

	CTeeHistorian();

	void Reset(const CGameInfo *pGameInfo, WRITE_CALLBACK pfnWriteCallback, void *pUser);
	void Finish();

	bool Starting() const { return m_State == STATE_START; }

	void BeginTick(int Tick);

	void BeginPlayers();
	void RecordPlayer(int ClientId, const CNetObj_CharacterCore *pChar);
	void RecordDeadPlayer(int ClientId);
	void RecordPlayerTeam(int ClientId, int Team);
	void RecordTeamPractice(int Team, bool Practice);
	void EndPlayers();

	void BeginInputs();
	void RecordPlayerInput(int ClientId, uint32_t UniqueClientId, const CNetObj_PlayerInput *pInput);
	void RecordPlayerMessage(int ClientId, const void *pMsg, int MsgSize);
	void RecordPlayerJoin(int ClientId, int Protocol);
	void RecordPlayerRejoin(int ClientId);
	void RecordPlayerReady(int ClientId);
	void RecordPlayerDrop(int ClientId, const char *pReason);
	void RecordPlayerName(int ClientId, const char *pName);
	void RecordConsoleCommand(int ClientId, int FlagMask, const char *pCmd, IConsole::IResult *pResult);
	void RecordTestExtra();
	void RecordPlayerSwap(int ClientId1, int ClientId2);
	void RecordTeamSaveSuccess(int Team, CUuid SaveId, const char *pTeamSave);
	void RecordTeamSaveFailure(int Team);
	void RecordTeamLoadSuccess(int Team, CUuid SaveId, const char *pTeamSave);
	void RecordTeamLoadFailure(int Team);
	void EndInputs();

	void EndTick();

	void RecordDDNetVersionOld(int ClientId, int DDNetVersion);
	void RecordDDNetVersion(int ClientId, CUuid ConnectionId, int DDNetVersion, const char *pDDNetVersionStr);

	void RecordAuthInitial(int ClientId, int Level, const char *pAuthName);
	void RecordAuthLogin(int ClientId, int Level, const char *pAuthName);
	void RecordAuthLogout(int ClientId);

	void RecordAntibot(const void *pData, int DataSize);

	void RecordPlayerFinish(int ClientId, int TimeTicks);
	void RecordTeamFinish(int TeamId, int TimeTicks);

	int m_Debug; // Possible values: 0, 1, 2.

private:
	void WriteHeader(const CGameInfo *pGameInfo);
	void WriteExtra(CUuid Uuid, const void *pData, int DataSize);
	void EnsureTickWrittenPlayerData(int ClientId);
	void EnsureTickWritten();
	void WriteTick();
	void Write(const void *pData, int DataSize);

	enum
	{
		STATE_START,
		STATE_BEFORE_TICK,
		STATE_BEFORE_PLAYERS,
		STATE_PLAYERS,
		STATE_BEFORE_INPUTS,
		STATE_INPUTS,
		STATE_BEFORE_ENDTICK,
		NUM_STATES,
	};

	struct CTeehistorianPlayer
	{
		bool m_Alive;
		int m_X;
		int m_Y;

		CNetObj_PlayerInput m_Input;
		uint32_t m_UniqueClientId;

		// DDNet team
		int m_Team;
	};

	struct CTeam
	{
		bool m_Practice;
	};

	WRITE_CALLBACK m_pfnWriteCallback;
	void *m_pWriteCallbackUserdata;

	int m_State;

	int m_LastWrittenTick;
	bool m_TickWritten;
	int m_Tick;
	int m_PrevMaxClientId;
	int m_MaxClientId;
	CTeehistorianPlayer m_aPrevPlayers[MAX_CLIENTS];
	CTeam m_aPrevTeams[MAX_CLIENTS];
};

#endif // GAME_SERVER_TEEHISTORIAN_H
