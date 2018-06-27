#include <base/hash.h>
#include <engine/console.h>
#include <engine/shared/fetcher.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>

#include <time.h>

struct CConfiguration;
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

		const char *m_pServerName;
		int m_ServerPort;
		const char *m_pGameType;

		const char *m_pMapName;
		int m_MapSize;
		SHA256_DIGEST m_MapSha256;
		int m_MapCrc;

		CConfiguration *m_pConfig;
		CTuningParams *m_pTuning;
		CUuidManager *m_pUuids;
	};

	CTeeHistorian();

	void Reset(const CGameInfo *pGameInfo, WRITE_CALLBACK pfnWriteCallback, void *pUser);
	void Finish();

	bool Starting() const { return m_State == STATE_START; }

	void BeginTick(int Tick);

	void BeginPlayers();
	void RecordPlayer(int ClientID, const CNetObj_CharacterCore *pChar);
	void RecordDeadPlayer(int ClientID);
	void EndPlayers();

	void BeginInputs();
	void RecordPlayerInput(int ClientID, const CNetObj_PlayerInput *pInput);
	void RecordPlayerMessage(int ClientID, const void *pMsg, int MsgSize);
	void RecordPlayerJoin(int ClientID);
	void RecordPlayerDrop(int ClientID, const char *pReason);
	void RecordConsoleCommand(int ClientID, int FlagMask, const char *pCmd, IConsole::IResult *pResult);
	void RecordTestExtra();
	void EndInputs();

	void EndTick();

	void RecordAuthInitial(int ClientID, int Level, const char *pAuthName);
	void RecordAuthLogin(int ClientID, int Level, const char *pAuthName);
	void RecordAuthLogout(int ClientID);

	int m_Debug; // Possible values: 0, 1, 2.

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

	struct CPlayer
	{
		bool m_Alive;
		int m_X;
		int m_Y;

		CNetObj_PlayerInput m_Input;
		bool m_InputExists;
	};

	WRITE_CALLBACK m_pfnWriteCallback;
	void *m_pWriteCallbackUserdata;

	int m_State;

	int m_LastWrittenTick;
	bool m_TickWritten;
	int m_Tick;
	int m_PrevMaxClientID;
	int m_MaxClientID;
	CPlayer m_aPrevPlayers[MAX_CLIENTS];
};
