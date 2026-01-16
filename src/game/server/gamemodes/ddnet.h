/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_GAMEMODES_DDNET_H
#define GAME_SERVER_GAMEMODES_DDNET_H

#include <game/server/gamecontroller.h>

class CScore;

class CGameControllerDDNet : public IGameController
{
public:
	CGameControllerDDNet(class CGameContext *pGameServer);
	~CGameControllerDDNet() override;

	CScore *Score();

	void HandleCharacterTiles(class CCharacter *pChr, int MapIndex) override;
	void SetArmorProgress(CCharacter *pCharacter, int Progress) override;
	int SnapPlayerScore(int SnappingClient, CPlayer *pPlayer) override;
	CFinishTime SnapPlayerTime(int SnappingClient, CPlayer *pPlayer) override;
	CFinishTime SnapMapBestTime(int SnappingClient) override;

	void OnPlayerConnect(class CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;

	void OnReset() override;

	void Tick() override;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true) override;
};
#endif // GAME_SERVER_GAMEMODES_DDNET_H
