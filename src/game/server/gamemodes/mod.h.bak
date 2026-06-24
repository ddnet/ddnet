#ifndef GAME_SERVER_GAMEMODES_MOD_H
#define GAME_SERVER_GAMEMODES_MOD_H

#include <game/server/gamecontroller.h>

class CGameControllerMod : public IGameController
{
private:
	// 每棒持续时间（Tick数），默认为 5秒 * 50Tick/秒 = 250 Tick
	int m_aTeamRelayDurationTicks[NUM_DDRACE_TEAMS];

public:
	CGameControllerMod(class CGameContext *pGameServer);
	~CGameControllerMod() override;

	void Tick() override;

	//yirou
	// 设置队伍接力持续时间（Tick数）
	void SetTeamRelayDuration(int Team, int Ticks)
	{
		if(Team >= 0 && Team < NUM_DDRACE_TEAMS)
			m_aTeamRelayDurationTicks[Team] = Ticks;
	}

	// 获取队伍接力持续时间（Tick数）
	int GetTeamRelayDuration(int Team) const
	{
		if(Team >= 0 && Team < NUM_DDRACE_TEAMS)
			return m_aTeamRelayDurationTicks[Team];
		return 0;
	}

	// 获取队伍接力持续时间（秒数）
	int GetTeamRelayDurationSeconds(int Team) const
	{
		if(Team >= 0 && Team < NUM_DDRACE_TEAMS && GameServer())
			return m_aTeamRelayDurationTicks[Team] / GameServer()->Server()->TickSpeed();
		return 0;
	}
};
#endif // GAME_SERVER_GAMEMODES_MOD_H
