#ifndef GAME_SERVER_INSTAGIB_SQL_STATS_PLAYER_H
#define GAME_SERVER_INSTAGIB_SQL_STATS_PLAYER_H

#include <base/system.h>

class CSqlStatsPlayer
{
public:
	// kills, deaths and flag grabs/caps are tracked per round
	int m_Kills;
	int m_Deaths;
	// in zCatch wins and losses are only counted if enough players are connected
	int m_Wins;
	int m_Losses;
	// used to track accuracy in any gametype
	// in grenade, hammer, ninja and shotgun based gametypes the
	// accuracy can go over 100%
	// because one shot can have multiple hits
	int m_ShotsFired;
	int m_ShotsHit;

	// Will also be set if spree chat messages are turned off
	// this is the spree highscore
	// the players current spree is in CPlayer::m_Spree
	int m_BestSpree;

	// gctf only

	int m_FlagCaptures;
	int m_FlagGrabs;
	int m_FlaggerKills;

	// ictf only

	// TODO: these are not actually incremented yet
	int m_Wallshots;

	// fng

	// the current multi is in player.h
	int m_BestMulti;
	// zCatch only for now but will possibly be shared

	// TODO: this should probably be in the base stats
	//       any pvp mode could collect points for kills/caps/wins
	int m_Points; // TODO: this is not tracked yet

	int m_TicksCaught; // TODO: this is not tracked yet
	int m_TicksInGame; // TODO: this is not tracked yet

	void Reset()
	{
		// base for all gametypes
		m_Kills = 0;
		m_Deaths = 0;
		m_BestSpree = 0;
		m_Wins = 0;
		m_Losses = 0;
		m_ShotsFired = 0;
		m_ShotsHit = 0;

		// gametype specific
		m_FlagCaptures = 0;
		m_FlagGrabs = 0;
		m_FlaggerKills = 0;
		m_Wallshots = 0;
		m_BestMulti = 0;
		m_Points = 0;
		m_TicksCaught = 0;
		m_TicksInGame = 0;
	}

	void Merge(const CSqlStatsPlayer *pOther)
	{
		// base for all gametypes
		m_Kills += pOther->m_Kills;
		m_Deaths += pOther->m_Deaths;
		m_BestSpree = std::max(m_BestSpree, pOther->m_BestSpree);
		m_Wins += pOther->m_Wins;
		m_Losses += pOther->m_Losses;
		m_ShotsFired += pOther->m_ShotsFired;
		m_ShotsHit += pOther->m_ShotsHit;

		// gametype specific
		m_FlagCaptures += pOther->m_FlagCaptures;
		m_FlagGrabs += pOther->m_FlagGrabs;
		m_FlaggerKills += pOther->m_FlaggerKills;
		m_Wallshots += pOther->m_Wallshots;
		m_BestMulti = std::max(m_BestMulti, pOther->m_BestMulti);
		m_Points += pOther->m_Points;
		m_TicksCaught += pOther->m_TicksCaught;
		m_TicksInGame += pOther->m_TicksInGame;
	}

	void Dump(const char *pSystem = "stats") const
	{
		dbg_msg(pSystem, "  kills: %d", m_Kills);
		dbg_msg(pSystem, "  deaths: %d", m_Deaths);
		dbg_msg(pSystem, "  spree: %d", m_BestSpree);
		dbg_msg(pSystem, "  wins: %d", m_Wins);
		dbg_msg(pSystem, "  losses: %d", m_Losses);
		dbg_msg(pSystem, "  shots_fired: %d", m_ShotsFired);
		dbg_msg(pSystem, "  shots_hit: %d", m_ShotsHit);

		dbg_msg(pSystem, "  flag_captures: %d", m_FlagCaptures);
		dbg_msg(pSystem, "  flag_grabs: %d", m_FlagGrabs);
		dbg_msg(pSystem, "  flagger_kills: %d", m_FlaggerKills);
		dbg_msg(pSystem, "  wallshots: %d", m_Wallshots);
		dbg_msg(pSystem, "  multi: %d", m_BestMulti);
		dbg_msg(pSystem, "  points: %d", m_Points);
		dbg_msg(pSystem, "  ticks_caught: %d", m_TicksCaught);
		dbg_msg(pSystem, "  ticks_in_game: %d", m_TicksInGame);
	}

	bool HasValues() const
	{
		return m_Kills ||
		       m_Deaths ||
		       m_BestSpree ||
		       m_Wins ||
		       m_Losses ||
		       m_ShotsFired ||
		       m_ShotsHit ||
		       m_FlagCaptures ||
		       m_FlagGrabs ||
		       m_FlaggerKills ||
		       m_Wallshots ||
		       m_BestMulti ||
		       m_Points ||
		       m_TicksCaught ||
		       m_TicksInGame;
	}

	CSqlStatsPlayer()
	{
		Reset();
	}
};

#endif
