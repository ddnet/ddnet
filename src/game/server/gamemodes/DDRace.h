/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_GAMEMODES_DDRACE_H
#define GAME_SERVER_GAMEMODES_DDRACE_H

#include <game/server/gamecontroller.h>

struct CScoreLoadBestTimeResult;
class CGameControllerDDRace : public IGameController
{
public:
	CGameControllerDDRace(class CGameContext *pGameServer);
	~CGameControllerDDRace();

	CScore *Score();

	void HandleCharacterTiles(class CCharacter *pChr, int MapIndex) override;

	void OnPlayerConnect(class CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;

	void OnReset() override;

	void Tick() override;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true) override;

	CClientMask GetMaskForPlayerWorldEvent(int Asker, int ExceptID = -1) override;

	std::shared_ptr<CScoreLoadBestTimeResult> m_pLoadBestTimeResult;
};
#endif // GAME_SERVER_GAMEMODES_DDRACE_H
