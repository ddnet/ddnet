#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_ZCATCH_ZCATCH_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_ZCATCH_ZCATCH_H

#include <game/server/instagib/extra_columns.h>
#include <game/server/instagib/sql_stats_player.h>

#include "../base_instagib.h"

class CZCatchColumns : public CExtraColumns
{
public:
	const char *CreateTable() override
	{
		return
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) sql_name "  " sql_type "  DEFAULT " default ","
#include "sql_columns.h"
#undef MACRO_ADD_COLUMN
			;
	}

	const char *SelectColumns() override
	{
		return
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) ", " sql_name
#include "sql_columns.h"
#undef MACRO_ADD_COLUMN
			;
	}

	const char *InsertColumns() override { return SelectColumns(); }

	const char *UpdateColumns() override
	{
		return
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) ", " sql_name " = ? "
#include "sql_columns.h"
#undef MACRO_ADD_COLUMN
			;
	}

	const char *InsertValues() override
	{
		return
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) ", ?"
#include "sql_columns.h"
#undef MACRO_ADD_COLUMN
			;
	}

	void InsertBindings(int *pOffset, IDbConnection *pSqlServer, const CSqlStatsPlayer *pStats) override
	{
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) pSqlServer->Bind##bind_type((*pOffset)++, pStats->m_##name);
#include "sql_columns.h"
#undef MACRO_ADD_COLUMN
	}

	void UpdateBindings(int *pOffset, IDbConnection *pSqlServer, const CSqlStatsPlayer *pStats) override
	{
		InsertBindings(pOffset, pSqlServer, pStats);
	}

	void Dump(const CSqlStatsPlayer *pStats, const char *pSystem = "stats") const override
	{
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) \
	dbg_msg(pSystem, "  %s: %d", sql_name, pStats->m_##name);
#include "sql_columns.h"
#undef MACRO_ADD_COLUMN
	}

	void MergeStats(CSqlStatsPlayer *pOutputStats, const CSqlStatsPlayer *pNewStats) override
	{
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) \
	pOutputStats->m_##name = Merge##bind_type##merge_method(pOutputStats->m_##name, pNewStats->m_##name);
#include "sql_columns.h"
#undef MACRO_ADD_COLUMN
	}

	void ReadAndMergeStats(int *pOffset, IDbConnection *pSqlServer, CSqlStatsPlayer *pOutputStats, const CSqlStatsPlayer *pNewStats) override
	{
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) \
	pOutputStats->m_##name = Merge##bind_type##merge_method(pSqlServer->Get##bind_type((*pOffset)++), pNewStats->m_##name); \
	dbg_msg("gctf", "db[%d]=%d round=%d => merge=%d", (*pOffset) - 1, pSqlServer->Get##bind_type((*pOffset) - 1), pNewStats->m_##name, pOutputStats->m_##name);
#include "sql_columns.h"
#undef MACRO_ADD_COLUMN
	}
};

class CGameControllerZcatch : public CGameControllerInstagib
{
public:
	CGameControllerZcatch(class CGameContext *pGameServer);
	~CGameControllerZcatch() override;

	int m_aBodyColors[MAX_CLIENTS] = {0};

	void KillPlayer(class CPlayer *pVictim, class CPlayer *pKiller);
	void OnCaught(class CPlayer *pVictim, class CPlayer *pKiller);
	void ReleasePlayer(class CPlayer *pPlayer, const char *pMsg);

	void Tick() override;
	void Snap(int SnappingClient) override;
	void OnPlayerConnect(CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pDisconnectingPlayer, const char *pReason) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	bool CanJoinTeam(int Team, int NotThisId, char *pErrorReason, int ErrorReasonSize) override;
	void DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg) override;
	bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number) override;
	bool DoWincheckRound() override;
	void OnRoundStart() override;
	bool OnSelfkill(int ClientId) override;
	int GetAutoTeam(int NotThisId) override;
	bool OnChangeInfoNetMessage(const CNetMsg_Cl_ChangeInfo *pMsg, int ClientId) override;
	int GetPlayerTeam(class CPlayer *pPlayer, bool Sixup) override;
	bool OnSetTeamNetMessage(const CNetMsg_Cl_SetTeam *pMsg, int ClientId) override;
	bool IsWinner(const CPlayer *pPlayer, char *pMessage, int SizeOfMessage) override;
	bool IsLoser(const CPlayer *pPlayer) override;

	enum class ECatchGameState
	{
		WAITING_FOR_PLAYERS,
		RELEASE_GAME,
		RUNNING,
		RUNNING_COMPETITIVE, // NEEDS 10 OR MORE TO START A REAL ROUND
	};
	ECatchGameState m_CatchGameState = ECatchGameState::WAITING_FOR_PLAYERS;
	ECatchGameState CatchGameState() const;
	void SetCatchGameState(ECatchGameState State);

	void CheckGameState();
	bool IsCatchGameRunning() const;

	// colors

	enum class ECatchColors
	{
		TEETIME,
		SAVANDER
	};
	ECatchColors m_CatchColors = ECatchColors::TEETIME;

	// gets the tee's body color based on the amount of its kills
	// the value is the integer that will be sent over the network
	int GetBodyColor(int Kills);

	int GetBodyColorTeetime(int Kills);
	int GetBodyColorSavander(int Kills);

	void SetCatchColors(class CPlayer *pPlayer);

	void SendSkinBodyColor7(int ClientId, int Color);

	void OnUpdateZcatchColorConfig() override;
};
#endif // GAME_SERVER_GAMEMODES_ZCATCH_H
