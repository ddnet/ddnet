#ifndef GAME_SERVER_INSTAGIB_SQL_STATS_PLAYER_H
#define GAME_SERVER_INSTAGIB_SQL_STATS_PLAYER_H

#include <base/system.h>

class CSqlStatsPlayer
{
public:
	// kills, deaths and flag grabs/caps are tracked per round
	int m_Kills;
	int m_Deaths;

	// Will also be set if spree chat messages are turned off
	int m_Spree;

	int m_FlagCaptures;
	int m_FlagGrabs;

	// fng

	// the current multi is in player.h
	int m_BestMulti;

	void Reset()
	{
		m_Kills = 0;
		m_Deaths = 0;
		m_Spree = 0;
		m_FlagCaptures = 0;
		m_FlagGrabs = 0;
		m_BestMulti = 0;
	}

	void Merge(const CSqlStatsPlayer *pOther)
	{
		m_Kills += pOther->m_Kills;
		m_Deaths += pOther->m_Deaths;
		m_Spree += pOther->m_Spree;
		m_FlagCaptures += pOther->m_FlagCaptures;
		m_FlagGrabs += pOther->m_FlagGrabs;
		m_BestMulti = std::max(m_BestMulti, pOther->m_BestMulti);
	}

	void Dump(const char *pSystem = "stats") const
	{
		dbg_msg(pSystem, "  kills: %d", m_Kills);
		dbg_msg(pSystem, "  deaths: %d", m_Deaths);
		dbg_msg(pSystem, "  spree: %d", m_Spree);
		dbg_msg(pSystem, "  flag_captures: %d", m_FlagCaptures);
		dbg_msg(pSystem, "  flag_grabs: %d", m_FlagGrabs);
		dbg_msg(pSystem, "  multi: %d", m_BestMulti);
	}

	bool HasValues() const
	{
		return m_Kills ||
		       m_Deaths ||
		       m_Spree ||
		       m_FlagCaptures ||
		       m_FlagGrabs ||
		       m_BestMulti;
	}

	CSqlStatsPlayer()
	{
		Reset();
	}
};

#endif
