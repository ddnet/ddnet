/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

#include <base/vmath.h>
#include <engine/map.h>
#include <engine/shared/protocol.h>
#include <game/server/teams.h>

struct CScoreLoadBestTimeResult;

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class IGameController
{
	friend class CSaveTeam; // need access to GameServer() and Server()

	std::vector<vec2> m_avSpawnPoints[3];

	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

	CGameTeams m_Teams;

protected:
	CGameContext *GameServer() const { return m_pGameServer; }
	CConfig *Config() { return m_pConfig; }
	IServer *Server() const { return m_pServer; }

	void DoActivityCheck();

	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_FriendlyTeam = -1;
			m_Pos = vec2(100, 100);
		}

		vec2 m_Pos;
		bool m_Got;
		int m_FriendlyTeam;
		float m_Score;
	};

	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos, int DDTeam);
	void EvaluateSpawnType(CSpawnEval *pEval, int Type, int DDTeam);

	void ResetGame();

	char m_aMapWish[MAX_MAP_LENGTH];

	int m_RoundStartTick;
	int m_GameOverTick;
	int m_SuddenDeath;

	int m_Warmup;
	int m_RoundCount;

	int m_GameFlags;
	int m_UnbalancedTick;
	bool m_ForceBalanced;

public:
	const char *m_pGameType;

	IGameController(class CGameContext *pGameServer);
	virtual ~IGameController();

	// event
	/*
		Function: OnCharacterDeath
			Called when a CCharacter in the world dies.

		Arguments:
			victim - The CCharacter that died.
			killer - The player that killed it.
			weapon - What weapon that killed it. Can be -1 for undefined
				weapon when switching team or player suicides.
	*/
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	/*
		Function: OnCharacterSpawn
			Called when a CCharacter spawns into the game world.

		Arguments:
			chr - The CCharacter that was spawned.
	*/
	virtual void OnCharacterSpawn(class CCharacter *pChr);

	virtual void HandleCharacterTiles(class CCharacter *pChr, int MapIndex);
	virtual void SetArmorProgress(CCharacter *pCharacer, int Progress){};

	/*
		Function: OnEntity
			Called when the map is loaded to process an entity
			in the map.

		Arguments:
			index - Entity index.
			pos - Where the entity is located in the world.

		Returns:
			bool?
	*/
	virtual bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number = 0);

	virtual void OnPlayerConnect(class CPlayer *pPlayer);
	virtual void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason);

	virtual void OnReset();

	// game
	virtual void DoWarmup(int Seconds);

	void StartRound();
	void EndRound();
	void ChangeMap(const char *pToMap);

	/*

	*/
	virtual bool CanBeMovedOnBalance(int ClientId);

	virtual void Tick();

	virtual void Snap(int SnappingClient);

	//spawn
	virtual bool CanSpawn(int Team, vec2 *pOutPos, int DDTeam);

	virtual void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true);
	/*

	*/
	virtual const char *GetTeamName(int Team);
	virtual int GetAutoTeam(int NotThisId);
	virtual bool CanJoinTeam(int Team, int NotThisId, char *pErrorReason, int ErrorReasonSize);
	int ClampTeam(int Team);

	CClientMask GetMaskForPlayerWorldEvent(int Asker, int ExceptID = -1);

	bool IsTeamPlay() { return m_GameFlags & GAMEFLAG_TEAMS; }
	// DDRace

	float m_CurrentRecord;
	CGameTeams &Teams() { return m_Teams; }
	std::shared_ptr<CScoreLoadBestTimeResult> m_pLoadBestTimeResult;
};

#endif
