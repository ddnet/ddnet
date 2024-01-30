#ifndef GAME_SERVER_INSTAGIB_ROUND_STATS_PLAYER_H
#define GAME_SERVER_INSTAGIB_ROUND_STATS_PLAYER_H

class CStatsPlayer
{
public:
	const char *m_pName;
	int m_Score;
	int m_Active;
	CStatsPlayer()
	{
		m_pName = "";
		m_Score = 0;
		m_Active = false;
	}
};

#endif
