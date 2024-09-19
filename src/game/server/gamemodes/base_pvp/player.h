#ifndef GAME_SERVER_GAMEMODES_BASE_PVP_PLAYER_H
// hack for headerguard linter
#endif

#ifndef IN_CLASS_PLAYER

#include <base/vmath.h>
#include <game/server/instagib/sql_stats.h>
#include <game/server/instagib/sql_stats_player.h>
#include <optional>
#include <vector>

// player object
class CPlayer
{
	std::optional<int> m_Score; // hack for IDEs
	int m_Team;
#endif // IN_CLASS_PLAYER

public:
	void InstagibTick();

	void ProcessStatsResult(CInstaSqlResult &Result);

	/*******************************************************************
	 * zCatch                                                          *
	 *******************************************************************/

	// Will be -1 when the player is alive
	int m_KillerId = -1;
	void SetTeamSpoofed(int Team, bool DoChatMsg = false);
	void SetTeamNoKill(int Team, bool DoChatMsg = false);
	void SetTeamRaw(int Team) { m_Team = Team; }
	// dead players can not respawn
	// will be used like m_RespawnDisabled in 0.7
	bool m_IsDead;
	bool m_GotRespawnInfo = false;
	bool m_WantsToJoinSpectators = false;
	std::vector<int> m_vVictimIds;

	/*******************************************************************
	 * gCTF                                                            *
	 *******************************************************************/
	bool m_GameStateBroadcast;
	int m_RespawnTick;
	bool m_IsReadyToEnter; // 0.7 ready change
	bool m_IsReadyToPlay; // 0.7 ready change
	bool m_DeadSpecMode; // 0.7 dead players

	/*******************************************************************
	 * fng                                                             *
	 *******************************************************************/
	int m_LastToucherId = -1;
	void UpdateLastToucher(int ClientId);

	int m_Multi = 1;

	int64_t m_LastKillTime = 0;
	int64_t HandleMulti();

	/*******************************************************************
	 * shared                                                          *
	 *******************************************************************/
	//Anticamper
	bool m_SentCampMsg;
	int m_CampTick;
	vec2 m_CampPos;

	// Will also be set if spree chat messages are turned off
	// this is the current spree
	// not to be confused with m_Stats.m_BestSpree which is the highscore
	int m_Spree;

	CSqlStatsPlayer m_Stats;
	int Spree() const { return m_Spree; }
	int Kills() const { return m_Stats.m_Kills; }
	int Deaths() const { return m_Stats.m_Deaths; }

	std::shared_ptr<CInstaSqlResult> m_StatsQueryResult;

	/*
		m_HasGhostCharInGame

		when the game starts for the first time
		and then a countdown starts there are no characters in game yet
		because they had no time to spawn

		but when the game reloads and tees were in game.
		those clients do not receive a new snap and still can see their tee.
		so their scoreboard is not forced on them.

		This variable marks those in game tees
	*/
	bool m_HasGhostCharInGame;

	/*
		m_IsFakeDeadSpec

		Marks players who are sent to the client as dead specs
		but in reality are real spectators.

		This is used to enable sv_spectator_votes for 0.7
		So the 0.7 clients think they are in game but in reality they are not.
	*/
	bool m_IsFakeDeadSpec = false;
	int64_t m_LastReadyChangeTick;
	void IncrementScore() { AddScore(1); }
	void DecrementScore() { AddScore(-1); }
	void AddScore(int Score) { m_Score = m_Score.value_or(0) + Score; }

#ifndef IN_CLASS_PLAYER
}
#endif
