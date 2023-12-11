#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_H

#include "DDRace.h"

class CGameControllerInstagib : public CGameControllerDDRace
{
public:
	CGameControllerInstagib(class CGameContext *pGameServer);
	~CGameControllerInstagib();

	void SendChatSpectators(const char *pMessage, int Flags);
	void OnPlayerConnect(CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;
	void DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void Tick() override;

	void AddSpree(CPlayer * pPlayer);
	void EndSpree(CPlayer * pPlayer, CPlayer * pKiller);

	// ddnet-insta only
	bool OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character) override;
};
#endif // GAME_SERVER_GAMEMODES_INSTAGIB_H
