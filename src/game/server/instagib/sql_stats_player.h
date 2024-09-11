#ifndef GAME_SERVER_INSTAGIB_SQL_STATS_PLAYER_H
#define GAME_SERVER_INSTAGIB_SQL_STATS_PLAYER_H

#include <base/system.h>

class CSqlStatsPlayer
{
public:
	// kills, deaths and flag grabs/caps are tracked per round
	int m_Kills;
	int m_Deaths;
	int m_Wins;
	int m_Losses;

	// Will also be set if spree chat messages are turned off
	// this is the spree highscore
	// the players current spree is in CPlayer::m_Spree
	int m_BestSpree;

	int m_FlagCaptures;
	int m_FlagGrabs;
	int m_FlaggerKills;

	// fng

	// the current multi is in player.h
	int m_BestMulti;

	void Reset()
	{
		m_Kills = 0;
		m_Deaths = 0;
		m_BestSpree = 0;
		m_Wins = 0;
		m_Losses = 0;
		m_FlagCaptures = 0;
		m_FlagGrabs = 0;
		m_FlaggerKills = 0;
		m_BestMulti = 0;
	}

	void Merge(const CSqlStatsPlayer *pOther)
	{
		m_Kills += pOther->m_Kills;
		m_Deaths += pOther->m_Deaths;
		m_BestSpree = std::max(m_BestSpree, pOther->m_BestSpree);
		m_Wins += pOther->m_Wins;
		m_Losses += pOther->m_Losses;
		m_FlagCaptures += pOther->m_FlagCaptures;
		m_FlagGrabs += pOther->m_FlagGrabs;
		m_FlaggerKills += pOther->m_FlaggerKills;
		m_BestMulti = std::max(m_BestMulti, pOther->m_BestMulti);
	}

	void Dump(const char *pSystem = "stats") const
	{
		dbg_msg(pSystem, "  kills: %d", m_Kills);
		dbg_msg(pSystem, "  deaths: %d", m_Deaths);
		dbg_msg(pSystem, "  spree: %d", m_BestSpree);
		dbg_msg(pSystem, "  wins: %d", m_Wins);
		dbg_msg(pSystem, "  losses: %d", m_Losses);
		dbg_msg(pSystem, "  flag_captures: %d", m_FlagCaptures);
		dbg_msg(pSystem, "  flag_grabs: %d", m_FlagGrabs);
		dbg_msg(pSystem, "  flagger_kills: %d", m_FlaggerKills);
		dbg_msg(pSystem, "  multi: %d", m_BestMulti);
	}

	bool HasValues() const
	{
		return m_Kills ||
		       m_Deaths ||
		       m_BestSpree ||
		       m_Wins ||
		       m_Losses ||
		       m_FlagCaptures ||
		       m_FlagGrabs ||
		       m_FlaggerKills ||
		       m_BestMulti;
	}

	CSqlStatsPlayer()
	{
		Reset();
	}
};

#endif
