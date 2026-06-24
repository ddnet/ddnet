#ifndef GAME_SERVER_GAMEMODES_MOD_H
#define GAME_SERVER_GAMEMODES_MOD_H

#include <game/server/gamecontroller.h>

class CGameControllerMod : public IGameController
{
public:
	CGameControllerMod(class CGameContext *pGameServer);
	~CGameControllerMod() override;

	void Tick() override;

	//yirou
	// relay duration is stored in CGameTeams; these helpers delegate to it
	void SetTeamRelayDuration(int Team, int Ticks)
	{
		Teams().SetTeamRelayDuration(Team, Ticks);
	}

	int GetTeamRelayDuration(int Team)
	{
		return Teams().GetTeamRelayDuration(Team);
	}

	int GetTeamRelayDurationSeconds(int Team)
	{
		int Ticks = Teams().GetTeamRelayDuration(Team);
		if(Ticks > 0 && GameServer())
			return Ticks / GameServer()->Server()->TickSpeed();
		return 0;
	}
};
#endif // GAME_SERVER_GAMEMODES_MOD_H
