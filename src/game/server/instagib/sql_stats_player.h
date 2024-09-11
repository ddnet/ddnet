#ifndef GAME_SERVER_INSTAGIB_SQL_STATS_PLAYER_H
#define GAME_SERVER_INSTAGIB_SQL_STATS_PLAYER_H

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

	void Reset()
	{
		m_Kills = 0;
		m_Deaths = 0;
		m_Spree = 0;
		m_FlagCaptures = 0;
		m_FlagGrabs = 0;
	}

	CSqlStatsPlayer()
	{
		Reset();
	}
};

#endif
