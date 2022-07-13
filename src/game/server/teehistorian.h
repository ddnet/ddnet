#ifndef GAME_SERVER_TEEHISTORIAN_H
#define GAME_SERVER_TEEHISTORIAN_H

#include <base/hash.h>
#include <engine/console.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>

#include <time.h>

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
		const char *m_pServerVersion = nullptr;
		time_t m_StartTime = 0;
		const char *m_pPrngDescription = nullptr;

		const char *m_pServerName = nullptr;
		int m_ServerPort = 0;
		const char *m_pGameType = nullptr;

		const char *m_pMapName = nullptr;
		int m_MapSize = 0;
		SHA256_DIGEST m_MapSha256;
		int m_MapCrc = 0;

		CConfig *m_pConfig = nullptr;
		CTuningParams *m_pTuning = nullptr;
		CUuidManager *m_pUuids = nullptr;
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
	void RecordPlayer(int ClientID, const CNetObj_CharacterCore *pChar);
	void RecordDeadPlayer(int ClientID);
	void RecordPlayerTeam(int ClientID, int Team);
	void RecordTeamPractice(int Team, bool Practice);
	void EndPlayers();

	void BeginInputs();
	void RecordPlayerInput(int ClientID, uint32_t UniqueClientID, const CNetObj_PlayerInput *pInput);
	void RecordPlayerMessage(int ClientID, const void *pMsg, int MsgSize);
	void RecordPlayerJoin(int ClientID, int Protocol);
	void RecordPlayerReady(int ClientID);
	void RecordPlayerDrop(int ClientID, const char *pReason);
	void RecordConsoleCommand(int ClientID, int FlagMask, const char *pCmd, IConsole::IResult *pResult);
	void RecordTestExtra();
	void RecordPlayerSwap(int ClientID1, int ClientID2);
	void RecordTeamSaveSuccess(int Team, CUuid SaveID, const char *pTeamSave);
	void RecordTeamSaveFailure(int Team);
	void RecordTeamLoadSuccess(int Team, CUuid SaveID, const char *pTeamSave);
	void RecordTeamLoadFailure(int Team);
	void EndInputs();

	void EndTick();

	void RecordDDNetVersionOld(int ClientID, int DDNetVersion);
	void RecordDDNetVersion(int ClientID, CUuid ConnectionID, int DDNetVersion, const char *pDDNetVersionStr);

	void RecordAuthInitial(int ClientID, int Level, const char *pAuthName);
	void RecordAuthLogin(int ClientID, int Level, const char *pAuthName);
	void RecordAuthLogout(int ClientID);

	int m_Debug = 0; // Possible values: 0, 1, 2.

private:
	void WriteHeader(const CGameInfo *pGameInfo);
	void WriteExtra(CUuid Uuid, const void *pData, int DataSize);
	void EnsureTickWrittenPlayerData(int ClientID);
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
		bool m_Alive = false;
		int m_X = 0;
		int m_Y = 0;

		CNetObj_PlayerInput m_Input;
		uint32_t m_UniqueClientID = 0;

		// DDNet team
		int m_Team = 0;
	};

	struct CTeam
	{
		bool m_Practice = false;
	};

	WRITE_CALLBACK m_pfnWriteCallback = nullptr;
	void *m_pWriteCallbackUserdata = nullptr;

	int m_State = 0;

	int m_LastWrittenTick = 0;
	bool m_TickWritten = false;
	int m_Tick = 0;
	int m_PrevMaxClientID = 0;
	int m_MaxClientID = 0;
	CTeehistorianPlayer m_aPrevPlayers[MAX_CLIENTS];
	CTeam m_aPrevTeams[MAX_CLIENTS];
};

#endif // GAME_SERVER_TEEHISTORIAN_H
